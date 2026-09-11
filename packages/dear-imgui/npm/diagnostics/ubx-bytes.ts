/** Deterministic UBX wire packets. Positions are synthetic, never receiver logs. */
export function ubxPacket(messageId: number, payload: Buffer, messageClass = 1): Buffer {
    const packet = Buffer.alloc(payload.length + 8);
    packet.set([0xb5, 0x62, messageClass, messageId]);
    packet.writeUInt16LE(payload.length, 4);
    packet.set(payload, 6);
    let a = 0, b = 0;
    for (const byte of packet.subarray(2, -2)) { a = (a + byte) & 255; b = (b + a) & 255; }
    packet[packet.length - 2] = a; packet[packet.length - 1] = b;
    return packet;
}
export const cnoValues = (sequence: number) => [10 + sequence % 10, 20 + Math.floor(sequence / 10) % 10,
    30 + Math.floor(sequence / 100) % 10, 40 + Math.floor(sequence / 1000) % 10];
export function navSatPacket(sequence: number) {
    const payload = Buffer.alloc(8 + 4 * 12);
    payload.writeUInt32LE(sequence * 50, 0); payload[4] = 1; payload[5] = 4;
    const cno = cnoValues(sequence);
    for (let i = 0; i < 4; ++i) {
        const offset = 8 + i * 12;
        payload[offset] = [0, 2, 3, 6][i]; payload[offset + 1] = i + 1;
        payload[offset + 2] = cno[i]; payload[offset + 3] = 30 + i * 10;
        payload.writeInt16LE(10 + i * 90 + sequence % 15, offset + 4);
        payload.writeUInt32LE(8, offset + 8);
    }
    return ubxPacket(0x35, payload);
}
export function navPvtPacket(sequence: number) {
    const payload = Buffer.alloc(92);
    payload.writeUInt32LE(sequence * 50, 0);
    payload.writeUInt16LE(2026, 4); payload[6] = 9; payload[7] = 11;
    payload[8] = 12; payload[9] = Math.floor(sequence / 1200) % 60; payload[10] = Math.floor(sequence / 20) % 60;
    payload[11] = 7; payload.writeUInt32LE(100, 12);
    payload[20] = 3; payload[21] = 1; payload[23] = 4;
    payload.writeInt32LE(-1200000 + sequence * 10, 24);
    payload.writeInt32LE(515000000 + (sequence % 100) * 10, 28);
    payload.writeInt32LE(80000 + sequence, 32); payload.writeInt32LE(50000 + sequence, 36);
    payload.writeUInt32LE(30000, 40); payload.writeUInt32LE(40000, 44);
    payload.writeInt32LE(1000 + sequence, 60); payload.writeInt32LE(9000000, 64);
    payload.writeUInt32LE(100, 68); payload.writeUInt32LE(1000, 72); payload.writeUInt16LE(120, 76);
    return ubxPacket(7, payload);
}
