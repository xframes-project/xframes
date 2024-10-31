import { ReplaySubject } from "rxjs";
import {
    CryptoBar,
    CryptoQuote,
    CryptoSnapshot,
} from "@alpacahq/alpaca-trade-api/dist/resources/datav2/entityv2";

export type CryptoQuoteWithSymbol = CryptoQuote & {
    S: string;
};

export type CryptoSnapshots = {
    [key: string]: CryptoSnapshot;
};

export type CryptoBarDataset = {
    [key: string]: CryptoBar[];
};

export class DataService {
    private cryptoQuotes: ReplaySubject<CryptoQuoteWithSymbol>;
    private cryptoSnapshots: ReplaySubject<CryptoSnapshots>;
    private cryptoBarDatasets: ReplaySubject<CryptoBarDataset>;

    constructor() {
        this.cryptoQuotes = new ReplaySubject(100000);
        this.cryptoSnapshots = new ReplaySubject(100000);
        this.cryptoBarDatasets = new ReplaySubject(100000);
    }

    addCryptoQuote(cryptoQuote: CryptoQuoteWithSymbol) {
        this.cryptoQuotes.next(cryptoQuote);
    }

    addCryptoSnapshots(cryptoSnapshots: CryptoSnapshots) {
        this.cryptoSnapshots.next(cryptoSnapshots);
    }

    addCryptoBars(cryptoBarDataset: CryptoBarDataset) {
        this.cryptoBarDatasets.next(cryptoBarDataset);
    }

    getCryptoQuotes() {
        return this.cryptoQuotes.asObservable();
    }

    getCryptoSnapshots() {
        return this.cryptoSnapshots.asObservable();
    }

    getCryptoBarDatasets() {
        return this.cryptoBarDatasets.asObservable();
    }
}
