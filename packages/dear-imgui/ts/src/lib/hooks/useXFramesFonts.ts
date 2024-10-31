import { useCallback, useMemo } from "react";
import { FontDef, Primitive } from "../components/XFrames/types";

export const useXFramesFonts = (fontDefs?: FontDef[]) => {
    const fonts = useMemo(
        () =>
            fontDefs
                ? fontDefs.map(({ name, sizes }) => sizes.map((size) => ({ name, size }))).flat()
                : [],
        [fontDefs],
    );

    return fonts;
};
