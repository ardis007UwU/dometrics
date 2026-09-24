{
  # Dometrics Nix flake: `nix profile install github:ardis007UwU/dometrics`
  # builds the C++20 CLI from source against nixpkgs' sqlite and places
  # `dometrics` on PATH. Supports Linux (x86_64/aarch64) and macOS
  # (x86_64/aarch64) via the system list below.
  description = "Dometrics — zero-maintenance developer telemetry CLI and daemon";

  inputs = {
    nixpkgs.url = "github:NixOS/nixpkgs/nixos-unstable";
  };

  outputs = { self, nixpkgs }:
    let
      systems = [ "x86_64-linux" "aarch64-linux" "x86_64-darwin" "aarch64-darwin" ];
      forEachSystem = nixpkgs.lib.genAttrs systems;
    in
    {
      packages = forEachSystem (system:
        let
          pkgs = nixpkgs.legacyPackages.${system};
          lib = nixpkgs.lib;
        in
        {
          dometrics = pkgs.stdenv.mkDerivation (finalAttrs: {
            pname = "dometrics";
            version = "1.0.0";
            # Filter out local build output so `nix build` stays pure and fast;
            # the flake source otherwise copies the whole working tree.
            src = lib.cleanSourceWith {
              src = ./.;
              filter = path: type:
                let base = baseNameOf path;
                in base != "build" && base != ".git" && base != "result";
            };

            nativeBuildInputs = [ pkgs.cmake ];
            buildInputs = [ pkgs.sqlite ];

            cmakeFlags = [ "-DCMAKE_BUILD_TYPE=Release" ];

            meta = {
              description = "Zero-maintenance developer telemetry CLI and background daemon";
              homepage = "https://github.com/ardis007UwU/dometrics";
              license = lib.licenses.mit;
              platforms = lib.platforms.unix;
              mainProgram = "dometrics";
            };
          });

          default = self.packages.${system}.dometrics;
        });

      apps = forEachSystem (system: {
        default = {
          type = "app";
          program = "${self.packages.${system}.default}/bin/dometrics";
        };
        dometrics = self.apps.${system}.default;
      });
    };
}
