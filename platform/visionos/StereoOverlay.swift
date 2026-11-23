// Stereoscopic depth for visionOS.
//
// SDL draws the far background (sky, scenery tiles, road background) into its
// window as usual. The engine also splits the road, sprites and the HUD into
// transparent layers (src/main/stereo_layers.hpp). This overlay sits on top of
// SDL's view and shows those layers pushed toward the viewer with SwiftUI's
// offset(z:), which visionOS renders with real stereoscopic depth.
//
// Depth follows the screen row: the road is cut into horizontal bands that get
// nearer toward the bottom of the screen, and each sprite is drawn just in
// front of the road band it stands on (its lowest row), so it sits on the road
// at the right distance and nearer road never covers it. The HUD is in front.
//
// Each nearer plane is scaled slightly so that, seen from a normal viewing
// distance in front of the window, it still lines up with the background.
// Only the layer images and their UIKit frames change per frame; the SwiftUI
// layout changes only when the depth setting does.

import SwiftUI
import UIKit

// Must match stereo_layers.hpp.
private let bands = 16
private let layerRoad = 0
private let firstSpriteLayer = 1
private let layerText = firstSpriteLayer + bands

// Nominal horizon (fraction of screen height) where depth starts.
private let nominalHorizon: CGFloat = 0.30
// Assumed viewing distance in points, for perspective compensation.
private let viewerDistance: CGFloat = 1300
// Nearest road depth in points for 3D DEPTH LOW / MEDIUM / HIGH.
private let maxDepthForLevel: [CGFloat] = [0, 60, 120, 200]
// Each road band extends this many points (in the window) down behind the
// nearer band below it, so no seam opens between bands at different depths
// when viewed off-centre. Extending only downward (behind) means a band never
// covers sprites standing on the band behind it.
private let bandOverlapPoints: CGFloat = 6
// Sprites sit just in front of their road band; the HUD in front of all.
private let spriteLift: CGFloat = 2
private let hudLift: CGFloat = 12

/// Depth fraction (0 = window plane, 1 = nearest) of a road band.
private func bandDepth(_ band: Int) -> CGFloat {
    let centre = (CGFloat(band) + 0.5) / CGFloat(bands)
    return max(0, min(1, (centre - nominalHorizon) / (1 - nominalHorizon)))
}

/// A full-size plane at one depth. The image lives in a child view whose
/// frame is set directly each frame.
final class StereoPlane: Identifiable {
    let id: Int
    let container = UIView()
    let content = UIView()
    let depth: CGFloat        // fraction of maxDepth
    let lift: CGFloat         // extra points toward the viewer

    init(id: Int, depth: CGFloat, lift: CGFloat) {
        self.id = id
        self.depth = depth
        self.lift = lift
        for v in [container, content] {
            v.isUserInteractionEnabled = false
            v.backgroundColor = .clear
        }
        container.clipsToBounds = false
        content.layer.magnificationFilter = .nearest
        content.layer.contentsGravity = .resize
        container.addSubview(content)
    }

    /// Show `image` covering rows [firstRow, firstRow + rows) of a frame that
    /// is `height` rows tall. `contentsRect` selects part of the image.
    func show(_ image: CGImage?, firstRow: Int, rows: Int, height: Int,
              contentsRect: CGRect = CGRect(x: 0, y: 0, width: 1, height: 1)) {
        guard let image, rows > 0, height > 0 else {
            content.layer.contents = nil
            return
        }
        let bounds = container.bounds
        let scale = bounds.height / CGFloat(height)
        content.frame = CGRect(x: 0, y: CGFloat(firstRow) * scale,
                               width: bounds.width, height: CGFloat(rows) * scale)
        content.layer.contentsRect = contentsRect
        content.layer.contents = image
    }
}

final class StereoModel: ObservableObject {
    @Published var maxDepth: CGFloat = 120
    let roadPlanes: [StereoPlane]
    let spritePlanes: [StereoPlane]
    let textPlane: StereoPlane
    let planes: [StereoPlane]   // back to front

    init() {
        var road: [StereoPlane] = []
        var sprites: [StereoPlane] = []
        var all: [StereoPlane] = []
        var id = 0
        for band in 0..<bands {
            let r = StereoPlane(id: id, depth: bandDepth(band), lift: 0); id += 1
            let s = StereoPlane(id: id, depth: bandDepth(band), lift: spriteLift); id += 1
            road.append(r); sprites.append(s)
        }
        // Back to front: all road bands, then sprites by band, then the HUD.
        all.append(contentsOf: road)
        all.append(contentsOf: sprites)
        let text = StereoPlane(id: id, depth: 1, lift: hudLift)
        all.append(text)
        roadPlanes = road
        spritePlanes = sprites
        textPlane = text
        planes = all
    }
}

/// Hosts a plane's container view.
struct PlaneHost: UIViewRepresentable {
    let view: UIView
    func makeUIView(context: Context) -> UIView { view }
    func updateUIView(_ uiView: UIView, context: Context) {}
}

struct StereoView: View {
    @ObservedObject var model: StereoModel

    var body: some View {
        GeometryReader { geometry in
            ZStack(alignment: .topLeading) {
                ForEach(model.planes) { plane in
                    let z = plane.depth * model.maxDepth + plane.lift
                    PlaneHost(view: plane.container)
                        .frame(width: geometry.size.width, height: geometry.size.height)
                        .scaleEffect((viewerDistance - z) / viewerDistance)
                        .offset(z: z)
                }
            }
        }
        .ignoresSafeArea()
        .allowsHitTesting(false)
    }
}

@MainActor
final class StereoOverlay {
    static let shared = StereoOverlay()

    private let model = StereoModel()
    private var host: UIHostingController<StereoView>?
    private let colorSpace = CGColorSpace(name: CGColorSpace.sRGB)!

    private func attachIfNeeded() {
        if let host, host.view.window != nil { return }
        guard let scene = UIApplication.shared.connectedScenes
                .compactMap({ $0 as? UIWindowScene }).first,
              let window = scene.windows.first(where: { $0.isKeyWindow }) ?? scene.windows.first,
              let root = window.rootViewController else { return }

        let host = UIHostingController(rootView: StereoView(model: model))
        host.view.backgroundColor = .clear
        host.view.isUserInteractionEnabled = false
        host.view.frame = root.view.bounds
        host.view.autoresizingMask = [.flexibleWidth, .flexibleHeight]
        root.addChild(host)
        root.view.addSubview(host.view)
        host.didMove(toParent: root)
        self.host = host
    }

    private func image(_ pixels: UnsafePointer<UInt32>?, width: Int, rows: Int) -> CGImage? {
        guard let pixels, width > 0, rows > 0 else { return nil }
        let data = Data(bytes: UnsafeRawPointer(pixels), count: width * rows * 4) as CFData
        guard let provider = CGDataProvider(data: data) else { return nil }
        return CGImage(width: width, height: rows,
                       bitsPerComponent: 8, bitsPerPixel: 32, bytesPerRow: width * 4,
                       space: colorSpace,
                       bitmapInfo: CGBitmapInfo(rawValue: CGImageAlphaInfo.premultipliedLast.rawValue),
                       provider: provider, decode: nil,
                       shouldInterpolate: false, intent: .defaultIntent)
    }

    func hide() {
        host?.view.isHidden = true
    }

    func update(layers: UnsafePointer<UnsafePointer<UInt32>?>,
                firstRows: UnsafePointer<Int32>, rowCounts: UnsafePointer<Int32>,
                count: Int, width: Int, height: Int, depthLevel: Int) {
        attachIfNeeded()
        guard let host, count > layerText else { return }
        host.view.isHidden = false
        if let root = host.parent?.view, root.subviews.last !== host.view {
            root.bringSubviewToFront(host.view)   // stay above SDL's views
        }

        let depth = maxDepthForLevel[max(0, min(maxDepthForLevel.count - 1, depthLevel))]
        if model.maxDepth != depth { model.maxDepth = depth }

        CATransaction.begin()
        CATransaction.setDisableActions(true)

        // Road: one image (rows firstRow..), shown band by band. Each band also
        // covers a few rows of the nearer band below it (converted from points
        // to rows), hidden behind that band except where a gap would open.
        let roadFirst = Int(firstRows[layerRoad]), roadRows = Int(rowCounts[layerRoad])
        let road = image(layers[layerRoad], width: width, rows: roadRows)
        let planeHeight = max(1, model.roadPlanes[0].container.bounds.height)
        let overlapRows = max(2, Int((bandOverlapPoints * CGFloat(height) / planeHeight).rounded(.up)))
        for band in 0..<bands {
            let bandFirst = band * height / bands
            let bandLast = min(height, (band + 1) * height / bands + overlapRows)   // exclusive
            let first = max(bandFirst, roadFirst)
            let last = min(bandLast, roadFirst + roadRows)
            if road == nil || last <= first {
                model.roadPlanes[band].show(nil, firstRow: 0, rows: 0, height: height)
                continue
            }
            let rect = CGRect(x: 0, y: CGFloat(first - roadFirst) / CGFloat(roadRows),
                              width: 1, height: CGFloat(last - first) / CGFloat(roadRows))
            model.roadPlanes[band].show(road, firstRow: first, rows: last - first,
                                        height: height, contentsRect: rect)
        }

        // Sprites, by the band they stand on.
        for band in 0..<bands {
            let l = firstSpriteLayer + band
            let rows = Int(rowCounts[l])
            model.spritePlanes[band].show(image(layers[l], width: width, rows: rows),
                                          firstRow: Int(firstRows[l]), rows: rows, height: height)
        }

        // HUD.
        let textRows = Int(rowCounts[layerText])
        model.textPlane.show(image(layers[layerText], width: width, rows: textRows),
                             firstRow: Int(firstRows[layerText]), rows: textRows, height: height)

        CATransaction.commit()
    }
}

/// Called from the renderer (main thread) once per presented frame.
@_cdecl("cb_visionos_stereo_update")
public func cb_visionos_stereo_update(_ layers: UnsafePointer<UnsafePointer<UInt32>?>?,
                                      _ firstRows: UnsafePointer<Int32>?,
                                      _ rowCounts: UnsafePointer<Int32>?,
                                      _ count: Int32, _ width: Int32, _ height: Int32,
                                      _ depthLevel: Int32, _ roadTop: Float) {
    MainActor.assumeIsolated {
        guard let layers, let firstRows, let rowCounts,
              count > 0, width > 0, height > 0 else {
            StereoOverlay.shared.hide()
            return
        }
        StereoOverlay.shared.update(layers: layers, firstRows: firstRows, rowCounts: rowCounts,
                                    count: Int(count), width: Int(width), height: Int(height),
                                    depthLevel: Int(depthLevel))
    }
}
