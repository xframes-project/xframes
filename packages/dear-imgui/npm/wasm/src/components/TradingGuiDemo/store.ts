import { create } from "zustand";
import { bind } from "@react-rxjs/core";
import { BehaviorSubject } from "rxjs";
import { cryptoSymbols } from "./cryptoSymbols";
import type { CryptoQuote } from "@alpacahq/alpaca-trade-api/dist/resources/datav2/entityv2.d.ts";

// const [textChange$, setText] = createSignal<string>();

// const [useText, text$] = bind(textChange$, "");

type CryptoQuoteSubject = BehaviorSubject<CryptoQuote>;
type CryptoQuoteSubjectMap = Map<string, CryptoQuoteSubject>;

const cryptoQuoteSubjectMap$ = new BehaviorSubject<CryptoQuoteSubjectMap>(new Map());
export const onLoadCryptoQuote = (symbol: string, cryptoQuote: CryptoQuote) => {
    console.log(symbol, cryptoQuote);

    const map = cryptoQuoteSubjectMap$.getValue();

    if (!map.has(symbol)) {
        map.set(symbol, new BehaviorSubject<CryptoQuote>(cryptoQuote));
    } else {
        const subject$ = map.get(symbol);
        subject$?.next(cryptoQuote);
        // map.set(symbol, subject$ as CryptoQuoteSubject);
    }

    cryptoQuoteSubjectMap$.next(map);
};

export const emptyCryptoQuote = Object.freeze({
    Timestamp: "",
    BidPrice: 0,
    BidSize: 0,
    AskPrice: 0,
    AskSize: 0,
});

export type State = {
    symbols: string[];
    cryptoAssets: any[];
    selectedTabIndex: number;
    setCryptoAssets: (cryptoAssets: any[]) => void;
    setSelectedTabIndex: (index: number) => void;
};

export const useStore = create<State>((set) => ({
    cryptoAssets: [],
    symbols: cryptoSymbols,
    selectedTabIndex: 0,
    setCryptoAssets: (cryptoAssets) => set(() => ({ cryptoAssets })),
    setSelectedTabIndex: (index) => set(() => ({ selectedTabIndex: index })),
}));
