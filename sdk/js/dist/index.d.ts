export interface TraceNode {
    level: number;
    identifier: number;
    chi: number;
    phi: number;
    score: number;
}
export interface WorkingMemorySlot {
    q: number;
    tau: number;
    kappa: number;
    weight: number;
    tags: string[];
}
export interface AgentStepResponse {
    q: number;
    modulated_q: number;
    chi: number;
    phi: number;
    score: number;
    best_id: number;
    beam: number;
    depth: number;
    trace: TraceNode[];
    working_memory: WorkingMemorySlot[];
    theta: Record<string, unknown>;
    timestamp: number;
}
export interface AgentStateResponse {
    theta: number[];
    pi: number[];
    rho: number[];
    sigma: number;
    updates: number;
    ema_reward: number;
    working_memory: WorkingMemorySlot[];
}
export declare class KolibriAgent {
    private baseUrl;
    constructor(baseUrl?: string);
    step(payload: {
        q: number | string;
        beam?: number;
        depth?: number;
        tags?: string[];
    }): Promise<AgentStepResponse>;
    state(): Promise<AgentStateResponse>;
    private prefix;
}
