import path from "node:path";
import { parse } from "yaml";
import { existsSync, readFileSync, writeFileSync } from "fs";

const pathToFaIconsYaml = path.join(
    "node_modules",
    "@fortawesome",
    "fontawesome-free",
    "metadata",
    "icons.yml",
);

const destPath = path.join("src", "lib", "fa-icons.ts");

const runner = async () => {
    if (existsSync(pathToFaIconsYaml)) {
        let faIconsYamlContents = readFileSync(pathToFaIconsYaml, { encoding: "utf8" });

        const faIcons = parse(faIconsYamlContents);

        const faIconMap: any = {};

        Object.keys(faIcons).forEach((key) => {
            faIconMap[key] = "\\u{" + faIcons[key].unicode + "}";
        });

        const faIconMapContents = `
const faIconMap = ${JSON.stringify(faIconMap, null, 4)} as const;

export default faIconMap;
        `;

        writeFileSync(destPath, faIconMapContents.replaceAll("\\\\", "\\"), {
            encoding: "utf8",
        });
    }
};

runner();
