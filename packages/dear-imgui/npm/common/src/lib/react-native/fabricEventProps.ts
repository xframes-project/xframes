// Non-enumerable payload metadata belongs to the prospective description, never
// a global render-attempt registry. JSON/native props see only upstream payloads.
export const eventPropsKey = Symbol("xframes.prospectiveEventProps");
export type EventProps = Readonly<Record<string, unknown>>;

export function captureEventProps(props: Record<string, unknown>, attributes?: Record<string, unknown>): EventProps {
    const events: Record<string, unknown> = {};
    for (const key of Object.keys(props)) {
        if (/^on[A-Z]/.test(key) && (!attributes || attributes[key])) events[key] = props[key];
    }
    return Object.freeze(events);
}

export function readEventProps(payload: any): EventProps | undefined {
    return payload?.[eventPropsKey];
}

export function withEventProps<T extends Record<string, unknown> | null>(payload: T, events: EventProps): T {
    if (payload) Object.defineProperty(payload, eventPropsKey, { value: events });
    return payload;
}

export function eventsDiffer(previous: EventProps, next: EventProps): boolean {
    return [...new Set([...Object.keys(previous), ...Object.keys(next)])].some(key => previous[key] !== next[key]);
}
