const { CleanWebpackPlugin } = require("clean-webpack-plugin");
const HtmlWebpackPlugin = require("html-webpack-plugin");
const MiniCssExtractPlugin = require("mini-css-extract-plugin");
const webpack = require("webpack");
const path = require("path");
const { execFileSync } = require("node:child_process");
const { readFileSync } = require("node:fs");
const os = require("node:os");
const diagnostics = process.env.XFRAMES_DIAGNOSTICS === "1";

const mode =
    process.env.NODE_ENV === "production" ? "production" : "development";

module.exports = [
    {
        name: "main",
        entry: path.resolve(__dirname, diagnostics ? "../diagnostics/browser-entry.ts" : "./src/index.tsx"),
        mode,
        // A diagnostic run measures one immutable bundle selected at startup.
        ...(diagnostics ? { watchOptions: { ignored: "**/*" } } : {}),
        devServer: {
            host: "127.0.0.1",
            port: diagnostics ? 3011 : 3000,
            ...(diagnostics ? { hot: false, liveReload: false, client: false } : { client: { overlay: { warnings: false } } }),
            open: true,
            static: [
                { directory: path.resolve(__dirname, "public") },
                { directory: path.resolve(__dirname, "../../assets"), publicPath: "/assets" },
            ],
            headers: {
                "Access-Control-Allow-Origin": "*",
                "Cross-Origin-Embedder-Policy": "require-corp",
                "Cross-Origin-Opener-Policy": "same-origin",
            },
        },
        output: {
            path: path.resolve(__dirname, diagnostics ? "build/fixture-app" : "build"),
            publicPath: "/",
        },
        experiments: {
            asyncWebAssembly: true,
            syncWebAssembly: true,
        },
        resolve: {
            extensions: [".ts", ".tsx", ".js", ".mjs", ".css"],
            modules: [
                path.resolve(__dirname, "./"),
                path.resolve(__dirname, "../node_modules"),
                "node_modules",
            ],
        },
        module: {
            rules: [
                {
                    test: /\.(tsx|ts)$/,
                    include: [path.resolve(__dirname, "src"), path.resolve(__dirname, "../diagnostics")],
                    exclude: /node_modules/,
                    loader: "ts-loader",
                    options: { transpileOnly: true, compilerOptions: { noEmit: false } },
                },
                {
                    test: /\.css$/i,
                    use: [MiniCssExtractPlugin.loader, "css-loader"],
                },
                {
                    test: /\.(ico|icns|eot|woff|woff2|jpe?g|png)$/,
                    use: [
                        {
                            loader: "url-loader",
                        },
                    ],
                },
                {
                    test: /\.(data)$/,
                    use: [
                        {
                            loader: "file-loader",
                            options: {
                                outputPath: "wasm",
                            },
                        },
                    ],
                },
            ],
        },
        plugins: [
            new MiniCssExtractPlugin({ ignoreOrder: true }),
            new HtmlWebpackPlugin({
                template: path.resolve(__dirname, "./public/index.html"),
            }),
            new CleanWebpackPlugin(),
            new webpack.DefinePlugin({
                "process.env.NODE_ENV": JSON.stringify(mode),
                ...(diagnostics ? {
                    XFRAMES_DIAGNOSTICS_OPTIONS: JSON.stringify(JSON.parse(process.env.XFRAMES_DIAGNOSTICS_OPTIONS ?? "{}")),
                    XFRAMES_SOURCE_REVISION: JSON.stringify(execFileSync("git", ["rev-parse", "HEAD"], { encoding: "utf8" }).trim()),
                    XFRAMES_HOST_INFO: JSON.stringify({ os: `${os.platform()} ${os.release()}`, cpu: os.cpus()[0]?.model,
                        node: process.version, sourceDirty: execFileSync("git", ["status", "--porcelain"], { encoding: "utf8" }).trim().length > 0 }),
                    XFRAMES_ADAPTER: JSON.stringify(process.env.XFRAMES_WEBGPU_ADAPTER ?? "swiftshader"),
                    XFRAMES_NATIVE_BUILD: JSON.stringify(/XFRAMES_FAST_BUILD:BOOL=ON/.test(readFileSync(path.resolve(__dirname, "../../cpp/wasm/build-wasm/CMakeCache.txt"), "utf8")) ? "fast-O0" : "optimized-O3"),
                } : {}),
            }),
        ],
    },
];
