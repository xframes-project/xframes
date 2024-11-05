import { XFrames } from "../../XFrames";

type HelpMarkerProps = {
    text: string;
};

export const HelpMarker = ({ text }: HelpMarkerProps) => (
    <XFrames.Node>
        <XFrames.DisabledText text="(?)" />
        <XFrames.ItemTooltip>
            <XFrames.TextWrap width={35 * 12}>
                <XFrames.UnformattedText text={text} />
            </XFrames.TextWrap>
        </XFrames.ItemTooltip>
    </XFrames.Node>
);
