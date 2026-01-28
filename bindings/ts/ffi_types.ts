// types for splinter snapshots


export type SplinterHeaderSnapshot = {
    magic: number,  
    version: number, 
    slots: number, 
    max_val_sz: number, 
    epoch: bigint,  
    auto_scrub: number,
    parse_failures: bigint,
    last_failure_epoch: bigint
};


export type SplinterSlotSnapshot = {
    hash: bigint,
    epoch: bigint,
    val_off: number,
    val_len: number,
    key: string
};