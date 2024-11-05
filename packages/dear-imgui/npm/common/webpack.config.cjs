const { CleanWebpackPlugin } = require("clean-webpack-plugin");
const HtmlWebpackPlugin = require("html-webpack-plugin");
const MiniCssExtractPlugin = require("mini-css-extract-plugin");
const webpack = require("webpack");
const path = require("path");

const { NODE_ENV } = process.env;

module.exports = [
    {
        name: "main",
        entry: path.resolve(__dirname, "./src/index.tsx"),
        mode: "development",
        devServer: {
            port: 3000,
            open: true,
            headers: {
                "Access-Control-Allow-Origin": "*",
                "Cross-Origin-Embedder-Policy": "require-corp",
                "Cross-Origin-Opener-Policy": "same-origin",
            },
        },
        output: {
            publicPath: "/",
        },
        experiments: {
            asyncWebAssembly: true,
            syncWebAssembly: true,
        },
        resolve: {
            extensions: [".ts", ".tsx", ".js", ".mjs", ".css"],
            modules: [path.resolve(__dirname, "./"), path.resolve(__dirname, "./node_modules/")],
        },
        module: {
            rules: [
                {
                    test: /\.(tsx|ts)$/,
                    include: [path.resolve(__dirname, "src")],
                    exclude: /node_modules/,
                    loader: "ts-loader",
                    options: { transpileOnly: true },
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
            new webpack.HotModuleReplacementPlugin(),
            new webpack.DefinePlugin({
                "process.env": {
                    // This has effect on the react lib size
                    NODE_ENV: JSON.stringify("development"),
                },
            }),
        ],
    },
];
