import { spawnSync } from "node:child_process";
import { createHash } from "node:crypto";
import { cpSync, existsSync, mkdirSync, readFileSync, writeFileSync } from "node:fs";
import { dirname, resolve } from "node:path";
import { fileURLToPath } from "node:url";

const here = dirname(fileURLToPath(import.meta.url));
const root = resolve(here, "..");
const repository = resolve(root, "../../..");
const args = Object.fromEntries(process.argv.slice(2).map(arg => {
    const match = /^--(source|output|base|packages|patch)=(.+)$/.exec(arg);
    if (!match) throw new Error(`Unknown argument: ${arg}`);
    return [match[1], match[2]];
}));
const source = resolve(args.source ?? "C:/dev/ubx-monitor");
const output = resolve(args.output ?? resolve(root, "build/diagnostics/ubx-application-reproduction"));
const app = resolve(output, "app");
const base = args.base ?? "migration";
if (!["migration", "qualified"].includes(base)) throw new Error("base must be migration or qualified");
if (args.patch && base !== "qualified") throw new Error("An incremental patch requires --base=qualified");
const baseRevision = base === "qualified" ? "e7de9e25ff3f608e8bb2613e471d0b97f47d58f2"
    : "571f5569bb923c3d4a8f37db8f8ada555323667a";
if (existsSync(app)) throw new Error(`Refusing to replace an existing checkout: ${app}`);
if (output === source || output.startsWith(source + "/") || output.startsWith(source + "\\"))
    throw new Error("The validation output must be outside the original application");
mkdirSync(output, { recursive: true });
const packages = resolve(output, "packages");
mkdirSync(packages);
const hash = file => createHash("sha256").update(readFileSync(file)).digest("hex");
const records = [];
function run(command, argv, cwd, label) {
    const result = spawnSync(command, argv, { cwd, encoding: "utf8", windowsHide: true, maxBuffer: 32 * 1024 * 1024 });
    records.push({ command, argv, cwd, label, status: result.status });
    writeFileSync(resolve(output, `${label}.log`), (result.stdout ?? "") + (result.stderr ?? ""));
    writeFileSync(resolve(output, "commands.json"), JSON.stringify(records, null, 2));
    if (result.error || result.status !== 0) throw result.error ?? new Error(`${label} failed; see ${output}`);
    return result.stdout;
}
const git = (argv, cwd, label) => run("git", argv, cwd, label);
const npmPath = process.env.npm_execpath ?? resolve(dirname(process.execPath), "node_modules/npm/bin/npm-cli.js");
if (!existsSync(npmPath)) throw new Error("Run via npm exec, or set npm_execpath to npm-cli.js");
const npm = (argv, cwd, label) => run(process.execPath, [npmPath, ...argv], cwd, label);
const preservedFiles = ["AGENTS.md", "package.json", "package-lock.json", "config.json"];
const original = Object.fromEntries(preservedFiles.filter(file => existsSync(resolve(source, file)))
    .map(file => [file, hash(resolve(source, file))]));
const sourceRevision = git(["rev-parse", "HEAD"], source, "original-revision").trim();
if (base === "migration" && sourceRevision !== baseRevision)
    throw new Error(`Application patch targets 571f556; inspect changed source ${sourceRevision} before migrating`);
const originalStatus = git(["status", "--porcelain=v1"], source, "original-status");
git(["diff", "--binary", "HEAD"], source, "original-diff");
const xframesRevision = git(["rev-parse", "HEAD"], repository, "xframes-revision").trim();
git(["status", "--porcelain=v1"], repository, "xframes-status");
git(["diff", "--binary", "HEAD"], repository, "xframes-diff");
const untracked = git(["ls-files", "--others", "--exclude-standard", "-z"], repository, "xframes-untracked")
    .split("\0").filter(Boolean);
const xframesUntracked = Object.fromEntries(untracked.map(file => [file, hash(resolve(repository, file))]));
// Native targets must already have been built from current source. Do not silently
// substitute a registry binary or refresh native builds just for a timestamp.
const native = resolve(root, "node/build/Release/xframes.node");
if (!existsSync(native)) throw new Error("Build the current Release Node target before setup");
let packed;
if (args.packages) {
    packed = ["xframes-common-0.1.7.tgz", "xframes-node-0.1.14.tgz"].map(filename => {
        const from = resolve(args.packages, filename);
        cpSync(from, resolve(packages, filename));
        return { filename, reusedFrom: from, sha256: hash(from) };
    });
} else {
npm(["run", "build:common"], root, "build-common");
npm(["run", "build:node"], root, "build-node-package");
packed = JSON.parse(npm(["pack", "--workspace", "@xframes/common", "--workspace", "@xframes/node",
    "--pack-destination", packages, "--json"], root, "pack"));
}
git(["clone", "--no-hardlinks", source, app], repository, "clone");
git(["checkout", "-b", base === "qualified" ? "qualification/ui-pacing" : "qualification/application", baseRevision], app, "checkout-base");
if (existsSync(resolve(source, "AGENTS.md"))) cpSync(resolve(source, "AGENTS.md"), resolve(app, "AGENTS.md"));
const patch = base === "migration" ? resolve(here, "ubx-application.patch") : args.patch ? resolve(args.patch) : null;
const patchSha256 = patch ? hash(patch) : null;
if (patch) {
git(["apply", "--check", patch], app, "patch-check");
git(["apply", patch], app, "patch-apply");
// Include newly added application sources in the retained dirty diff as well.
git(["add", "--intent-to-add", "src", "tests", "package.json", "package-lock.json"], app, "patch-new-files");
}
// Keep the committed lock's dependency selections; refresh only local tarball
// integrity because a rebuilt native artifact may have a different build identity.
npm(["install", "--package-lock-only", "--ignore-scripts", "--save-exact",
    "../packages/xframes-common-0.1.7.tgz", "../packages/xframes-node-0.1.14.tgz"], app, "local-lock");
npm(["ci"], app, "install");
npm(["run", "typecheck"], app, "typecheck");
npm(["run", "test:serial"], app, "serial-lifecycle");
if (JSON.parse(readFileSync(resolve(app, "package.json"), "utf8")).scripts["test:pacing"])
    npm(["run", "test:pacing"], app, "pacing");
if (hash(resolve(app, "node_modules/@xframes/node/dist/xframes.node")) !== hash(native))
    throw new Error("Installed package native binary does not match current Release target");
git(["diff", "--binary", "HEAD"], app, "application-diff");
for (const [file, before] of Object.entries(original)) {
    if (hash(resolve(source, file)) !== before) throw new Error(`Original ${file} changed during setup`);
}
if (git(["status", "--porcelain=v1"], source, "original-status-after") !== originalStatus)
    throw new Error("Original checkout status changed during setup");
const identities = {};
for (const file of ["assets/fonts/roboto-regular.ttf", "assets/fonts/roboto-mono.ttf", "src/scripts/sky-view.js",
    "package-lock.json", "node_modules/@xframes/node/dist/xframes.node"]) identities[file] = hash(resolve(app, file));
writeFileSync(resolve(output, "provenance.json"), JSON.stringify({ source, sourceRevision, baseRevision, base, original,
    xframesRevision, xframesUntracked, patchSha256, nativeSha256: hash(native), packages: packed,
    identities, node: process.version, nativeBuildRequirement: "Current-source Release build; retain build logs alongside this record" }, null, 2));
if (patch && hash(patch) !== patchSha256) throw new Error("Application patch changed during setup; repeat in a fresh output directory");
console.log(`Prepared ${app}; typecheck and serial lifecycle passed. Desktop execution is a separate explicit run.`);
