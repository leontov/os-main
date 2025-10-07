import { API_BASE_URL, buildUrl } from "./api";

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

export interface ThetaSnapshot {
  theta: number[];
  pi: number[];
  rho: number[];
  sigma: number;
  updates: number;
  ema_reward: number;
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
  theta: ThetaSnapshot;
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

export interface AgentStepRequest {
  q: string | number;
  beam?: number;
  depth?: number;
  tags?: string[];
}

export const runAgentStep = async (payload: AgentStepRequest): Promise<AgentStepResponse> => {
  const response = await fetch(buildUrl("/api/agent/step"), {
    method: "POST",
    headers: { "Content-Type": "application/json" },
    body: JSON.stringify(payload),
  });

  if (!response.ok) {
    const detail = await response.text();
    throw new Error(detail || "Не удалось выполнить шаг агента");
  }

  return (await response.json()) as AgentStepResponse;
};

export const fetchAgentState = async (): Promise<AgentStateResponse> => {
  const response = await fetch(buildUrl("/api/agent/state"));
  if (!response.ok) {
    throw new Error("Не удалось получить состояние агента");
  }
  return (await response.json()) as AgentStateResponse;
};
