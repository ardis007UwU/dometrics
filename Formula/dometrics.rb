class Dometrics < Formula
  desc "Zero-maintenance developer telemetry CLI and background daemon"
  homepage "https://github.com/Dominion-Studios/dometrics"
  url "https://github.com/Dominion-Studios/dometrics/archive/refs/tags/v1.0.0.tar.gz"
  sha256 "62e2101beb5435ea5b59acc115fd5a8baa95492b4f47cbde7d7b4c3824c01648"
  license "MIT"
  head "https://github.com/Dominion-Studios/dometrics.git", branch: "main"

  depends_on "cmake" => :build
  depends_on "sqlite"

  def install
    system "cmake", "-S", ".", "-B", "build",
           "-DCMAKE_BUILD_TYPE=Release",
           *std_cmake_args
    system "cmake", "--build", "build", "--parallel"
    bin.install "build/dometrics"
  end

  test do
    assert_match version.to_s, shell_output("#{bin}/dometrics --version")
    (testpath/"probe").mkpath
    cd "probe" do
      system "git", "init", "-q", "."
      system bin/"dometrics", "init", "--name", "brewtest"
      assert_match "brewtest", shell_output("#{bin}/dometrics summary brewtest")
    end
  end
end
