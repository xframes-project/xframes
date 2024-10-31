import { createContext, useContext } from "react";
import { DataService } from "./dataService";

export const DataServiceContext = createContext<DataService | null>(null);

export const useDataService = () => {
    const context = useContext(DataServiceContext);

    if (context) {
        return context;
    }

    throw new Error("DataServiceContext initialisation failed");
};
