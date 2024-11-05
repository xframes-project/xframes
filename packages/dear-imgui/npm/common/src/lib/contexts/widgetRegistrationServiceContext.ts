import { createContext } from "react";
import { WidgetRegistrationService } from "../widgetRegistrationService";

export const WidgetRegistrationServiceContext = createContext<WidgetRegistrationService | null>(
    null,
);
