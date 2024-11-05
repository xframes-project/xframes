const { CleanWebpackPlugin } = require("clean-webpack-plugin");
const HtmlWebpackPlugin = require("html-webpack-plugin");
const MiniCssExtractPlugin = require("mini-css-extract-plugin");
const webpack = require("webpack");
const path = require("path");

module.exports = {
    // entry: path.resolve(__dirname, "./src/lib/index.tsx"),
    mode: "development",
    experiments: {
        asyncWebAssembly: true,
        syncWebAssembly: true,
    },
    resolve: {
        extensions: [".ts", ".tsx", ".js", ".mjs", ".css"],
        modules: [path.resolve(__dirname, "./"), path.resolve(__dirname, "./node_modules/")],
    },
    entry: "./src/lib/index.tsx",
    devtool: "inline-source-map",
    output: {
        path: path.resolve(__dirname, "build"),
        // library: {
        //     name: "xframes",
        //     type: "umd",
        // },
    },
    module: {
        rules: [
            {
                test: /\.(tsx|ts)$/,
                include: [path.resolve(__dirname, "src")],
                exclude: /node_modules/,
                loader: "ts-loader",
                options: { transpileOnly: true, configFile: "build.tsconfig.json" },
            },
            {
                test: /\.css$/i,
                use: [MiniCssExtractPlugin.loader, "css-loader"],
            },
            {
                test: /\.(ico|icns|eot|woff|woff2|jpe?g|png)$/,
                // exclude: [path.resolve(__dirname, "react-native")],
                use: [
                    {
                        // loader: "asset/inline",
                        loader: "url-loader",
                    },
                ],
            },
        ],
    },
    plugins: [new MiniCssExtractPlugin({ ignoreOrder: true }), new CleanWebpackPlugin()],
};
