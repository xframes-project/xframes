import { ComponentProps } from "react";

type OtherComponents = {
    [key: string]: React.ComponentType<ComponentProps<any>>;
};

export function attachSubComponents<
    C extends React.ComponentType<ComponentProps<C>>,
    O extends OtherComponents,
>(displayName: string, topLevelComponent: C, otherComponents: O): C & O {
    topLevelComponent.displayName = displayName;
    Object.entries(otherComponents).forEach(
        ([key, component]) => (component.displayName = `${displayName}.${key}`),
    );

    return Object.assign(topLevelComponent, otherComponents);
}
