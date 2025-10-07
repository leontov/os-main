import fetch from "node-fetch";

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

export class KolibriAgent {
  constructor(private baseUrl: string = "http://127.0.0.1:8056") {}

  async step(payload: {
    q: number | string;
    beam?: number;
    depth?: number;
    tags?: string[];
  }): Promise<AgentStepResponse> {
    const response = await fetch(this.prefix("/api/agent/step"), {
      method: "POST",
      headers: { "Content-Type": "application/json" },
      body: JSON.stringify(payload),
    });
    if (!response.ok) {
      throw new Error(`step failed: ${response.status} ${response.statusText}`);
    }
    return (await response.json()) as AgentStepResponse;
  }

  async state(): Promise<AgentStateResponse> {
    const response = await fetch(this.prefix("/api/agent/state"));
    if (!response.ok) {
      throw new Error(`state failed: ${response.status} ${response.statusText}`);
    }
    return (await response.json()) as AgentStateResponse;
  }

  private prefix(endpoint: string): string {
    return `${this.baseUrl.replace(/\/$/, "")}${endpoint}`;
  }
}
