// swift-tools-version:5.1
//
import PackageDescription

let package = Package(
    name: "WalletKitCore",
    products: [
        .library(
            name: "WalletKitCore",
            targets: ["WalletKitCore"]
        ),
        
        .executable(
            name: "WalletKitCoreExplore",
            targets: ["WalletKitCoreExplore"]
        ),

        .executable(
            name: "WalletKitCorePerf",
            targets: ["WalletKitCorePerf"]
        ),
    ],
    dependencies: [],
    targets: [
        // MARK: - Core Targets
        .target(
            name: "WalletKitCore",
            dependencies: [
                "WalletKitCoreSafe",
                "WalletKitSQLite",
                "WalletKitEd25519",
                "WalletKitHederaProto",
                "WalletKitBlake2"
            ],
            path: ".",
            sources: ["src/version"],               // Holds BRCryptoVersion.c only
            publicHeadersPath: "include",           // Export all public includes
            linkerSettings: [
                .linkedLibrary("resolv"),
                .linkedLibrary("pthread"),
                .linkedLibrary("bsd", .when(platforms: [.linux])),
            ]
        ),

        .target(
            name: "WalletKitCoreSafe",
            dependencies: [
            ],
            path: "src",
            exclude: [
                "hedera/proto",      // See target: WalletKitHederaProto
                "version",
                "ethereum/bcs",
                "ethereum/les",
                "ethereum/mpt",
            ],
            publicHeadersPath: "version",   // A directory WITHOUT headers
            cSettings: [
                .headerSearchPath("../include"),           // BRCrypto
                .headerSearchPath("."),
                .headerSearchPath("../vendor"),
                .headerSearchPath("../vendor/secp256k1")  // To compile vendor/secp256k1/secp256k1.c
            ]
        ),

        // Custom compilation flags for SQLite - to silence warnings
        .target(
            name: "WalletKitSQLite",
            dependencies: [],
            path: "vendor/sqlite3",
            sources: ["sqlite3.c"],
            publicHeadersPath: "include"
        ),

        // Custom compilation flags for ed15519 - to silence warnings
        .target(
            name: "WalletKitEd25519",
            dependencies: [],
            path: "vendor/ed25519",
            exclude: [],
            publicHeadersPath: nil
        ),

        // Custom compilation flags for hedera/proto - to silence warnings
        .target(
            name: "WalletKitHederaProto",
            dependencies: [],
            path: "src/hedera/proto",
            publicHeadersPath: nil
        ),
        
        // Custom compilation flags for blake2 - to silence warnings
        .target(
            name: "WalletKitBlake2",
            dependencies: [],
            path: "vendor/blake2",
            exclude: [],
            publicHeadersPath: nil
        ),

        // MARK: - Core Misc Targets

        .target (
            name: "WalletKitCoreExplore",
            dependencies: ["WalletKitCore"],
            path: "WalletKitCoreExplore",
            cSettings: [
                .headerSearchPath("../include"),
                .headerSearchPath("../src"),
            ]
        ),

        .target (
            name: "WalletKitCorePerf",
            dependencies: ["WalletKitCore", "WalletKitCoreSupportTests"],
            path: "WalletKitCorePerf",
            cSettings: [
                .headerSearchPath("../include"),
                .headerSearchPath("../src"),
            ]
        ),

        // MARK: - Core Test Targets

        .target(
            name: "WalletKitCoreSupportTests",
            dependencies: ["WalletKitCore"],
            path: "WalletKitCoreTests/test",
            publicHeadersPath: "include",
            cSettings: [
                .define("BITCOIN_TEST_NO_MAIN"),
                .headerSearchPath("../../include"),
                .headerSearchPath("../../src"),
            ]
        ),

        .testTarget(
            name: "WalletKitCoreTests",
            dependencies: [
                "WalletKitCoreSupportTests"
            ],
            path: "WalletKitCoreTests",
            exclude: [
                "test"
            ],
            cSettings: [
                .headerSearchPath("../src"),
            ],
            linkerSettings: [
                .linkedLibrary("pthread"),
                .linkedLibrary("bsd", .when(platforms: [.linux])),
            ]
        ),
    ]
)
