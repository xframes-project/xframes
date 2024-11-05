import { useEffect, useRef } from "react";
import { v4 as uuidv4 } from "uuid";
import { WidgetRegistrationService } from "../widgetRegistrationService";
import { useWidgetRegistrationService } from "../hooks/useWidgetRegistrationService";

export const useWidgetEventManagement = (
    type?: "text" | "combo" | "numeric" | "boolean" | "multi",
): [React.MutableRefObject<string>, WidgetRegistrationService] => {
    const widgetRegistrationService = useWidgetRegistrationService();
    const idRef = useRef(uuidv4());

    // useEffect(() => {
    //     return () => {
    //         if (type === "text") {
    //             widgetRegistrationService.offTextInputChange(idRef.current);
    //         } else if (type === "combo") {
    //             widgetRegistrationService.offComboChange(idRef.current);
    //         } else if (type === "numeric") {
    //             widgetRegistrationService.offNumericValueChange(idRef.current);
    //         } else if (type === "boolean") {
    //             widgetRegistrationService.offBooleanValueChange(idRef.current);
    //         } else if (type === "multi") {
    //             widgetRegistrationService.offMultiValueChange(idRef.current);
    //         }
    //     };
    // }, [widgetRegistrationService]);

    return [idRef, widgetRegistrationService];
};
