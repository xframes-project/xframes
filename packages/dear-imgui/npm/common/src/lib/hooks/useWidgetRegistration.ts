import { useEffect, useLayoutEffect, useRef, useState } from "react";
import { WidgetRegistrationService, type WidgetTarget, type RegistrationKind } from "../widgetRegistrationService";

/** Capture the native owner during layout; passive setup and cleanup own a lease.
 * A late passive effect cannot resolve a reused public ID to a different owner.
 * Effect cleanup releases registration only; native acknowledgment owns liveness.
 */
export function useWidgetRegistration(service: WidgetRegistrationService, kind: RegistrationKind = "widget") {
    const [id] = useState(() => service.generateId());
    const target = useRef<WidgetTarget | undefined>(undefined);
    useLayoutEffect(() => {
        target.current = service.captureWidget(id);
        return () => { target.current = undefined; };
    }, [service, id]);
    useEffect(() => service.registerWidget(target.current, kind), [service, id, kind]);
    return { current: id };
}
