declare module "*.data" { const url: string; export default url; }
declare module "*.mjs" { const factory: (options: any) => Promise<any>; export default factory; }
