# fastlane for the CI's device sync and Firebase upload (.github/workflows/apple-builds.yml).
# Not needed for local builds.
source "https://rubygems.org"
gem "fastlane", "~> 2.228"
plugins_path = File.join(File.dirname(__FILE__), "fastlane", "Pluginfile")
eval_gemfile(plugins_path) if File.exist?(plugins_path)
