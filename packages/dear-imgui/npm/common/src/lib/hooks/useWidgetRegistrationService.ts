import { useContext } from "react";
import { WidgetRegistrationServiceContext } from "../contexts/widgetRegistrationServiceContext";

export const useWidgetRegistrationService = () => {
    const context = useContext(WidgetRegistrationServiceContext);

    if (context) {
        return context;
    }

    throw new Error("WidgetRegistrationServiceContext initialisation failed");
};
