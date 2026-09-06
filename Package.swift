// swift-tools-version: 6.0
import PackageDescription

let package = Package(
    name: "NeuroevoStudio",
    platforms: [.macOS(.v14)],
    products: [
        .executable(name: "NeuroevoStudio", targets: ["NeuroevoStudio"])
    ],
    targets: [
        .target(
            name: "CNeuroevo",
            path: ".",
            sources: ["src/neuroevo.cpp", "src/c_api.cpp"],
            publicHeadersPath: "native/CNeuroevo/include",
            cxxSettings: [
                .headerSearchPath("include"),
                .define("NEUROEVO_USE_ACCELERATE", to: "1"),
                .define("ACCELERATE_NEW_LAPACK", to: "1")
            ],
            linkerSettings: [.linkedFramework("Accelerate")]
        ),
        .executableTarget(
            name: "NeuroevoStudio",
            dependencies: ["CNeuroevo"],
            path: "native/NeuroevoStudio",
            exclude: ["Info.plist"],
            linkerSettings: [
                .linkedFramework("AppKit"),
                .linkedFramework("Charts"),
                .linkedFramework("CoreML"),
                .linkedFramework("SwiftUI")
            ]
        )
    ],
    cxxLanguageStandard: .cxx20
)
