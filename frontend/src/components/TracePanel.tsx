import { useCallback, useEffect, useState } from "react";
import { Activity, Play, RefreshCcw, Zap } from "lucide-react";
import type { AgentStepResponse, TraceNode, WorkingMemorySlot } from "../core/agent";
import { fetchAgentState, runAgentStep } from "../core/agent";

const formatNumber = (value: number, digits = 4) => Number.parseFloat(value.toFixed(digits));

const TracePanel = () => {
  const [q, setQ] = useState("42");
  const [beam, setBeam] = useState(12);
  const [depth, setDepth] = useState(6);
  const [loading, setLoading] = useState(false);
  const [error, setError] = useState<string | null>(null);
  const [response, setResponse] = useState<AgentStepResponse | null>(null);
  const [workingMemory, setWorkingMemory] = useState<WorkingMemorySlot[]>([]);

  const loadState = useCallback(async () => {
    try {
      const snapshot = await fetchAgentState();
      setWorkingMemory(snapshot.working_memory ?? []);
    } catch (stateError) {
      console.debug("[trace] state unavailable", stateError);
    }
  }, []);

  useEffect(() => {
    void loadState();
  }, [loadState]);

  const handleStep = useCallback(async () => {
    setLoading(true);
    setError(null);
    try {
      const result = await runAgentStep({ q, beam, depth });
      setResponse(result);
      setWorkingMemory(result.working_memory ?? []);
    } catch (stepError) {
      setError(stepError instanceof Error ? stepError.message : String(stepError));
    } finally {
      setLoading(false);
    }
  }, [q, beam, depth]);

  const handleReset = useCallback(() => {
    setResponse(null);
    setError(null);
    void loadState();
  }, [loadState]);

  const renderTrace = (trace: TraceNode[]) => {
    if (!trace.length) {
      return <p className="text-xs text-text-light">Трассировка пуста.</p>;
    }
    const top = trace.slice(0, 8);
    return (
      <div className="space-y-2">
        {top.map((node) => (
          <div key={`${node.level}-${node.identifier}`} className="rounded-xl bg-white/70 p-2 shadow-inner">
            <p className="text-xs text-text-light">Уровень {node.level}</p>
            <p className="text-sm font-mono text-text-dark">
              χ={formatNumber(node.chi)} Φ={formatNumber(node.phi)} S={formatNumber(node.score)}
            </p>
            <p className="text-[0.7rem] text-text-light">ID {node.identifier}</p>
          </div>
        ))}
      </div>
    );
  };

  return (
    <div className="rounded-3xl bg-white/70 p-4 shadow-card">
      <div className="flex items-center gap-2">
        <Zap className="h-4 w-4 text-primary" />
        <h3 className="text-sm font-semibold text-text-dark">Agent Step</h3>
      </div>
      <div className="mt-4 space-y-3">
        <label className="flex flex-col text-xs text-text-light">
          <span>Вход q</span>
          <input
            className="rounded-lg border border-border-light px-2 py-1 text-sm"
            value={q}
            onChange={(event) => setQ(event.target.value)}
          />
        </label>
        <div className="grid grid-cols-2 gap-3 text-xs text-text-light">
          <label className="flex flex-col">
            <span>Beam</span>
            <input
              type="number"
              min={1}
              max={256}
              className="rounded-lg border border-border-light px-2 py-1 text-sm"
              value={beam}
              onChange={(event) => setBeam(Number.parseInt(event.target.value, 10) || 1)}
            />
          </label>
          <label className="flex flex-col">
            <span>Depth</span>
            <input
              type="number"
              min={1}
              max={64}
              className="rounded-lg border border-border-light px-2 py-1 text-sm"
              value={depth}
              onChange={(event) => setDepth(Number.parseInt(event.target.value, 10) || 1)}
            />
          </label>
        </div>
        <div className="flex items-center gap-2">
          <button
            type="button"
            className="flex flex-1 items-center justify-center gap-2 rounded-xl bg-primary px-3 py-2 text-sm font-semibold text-white transition-opacity disabled:opacity-60"
            onClick={handleStep}
            disabled={loading}
          >
            <Play className="h-4 w-4" />
            Шаг
          </button>
          <button
            type="button"
            className="flex items-center gap-2 rounded-xl border border-border-light px-3 py-2 text-xs text-text-light transition-colors hover:border-primary hover:text-primary"
            onClick={handleReset}
          >
            <RefreshCcw className="h-3 w-3" />
          </button>
        </div>
        {error && <p className="text-xs text-accent-coral">{error}</p>}
        {response && (
          <div className="space-y-3 rounded-2xl bg-white/80 p-3">
            <div className="flex items-center gap-2 text-xs text-text-light">
              <Activity className="h-3 w-3 text-primary" />
              <span>χ={formatNumber(response.chi)} Φ={formatNumber(response.phi)} S={formatNumber(response.score)}</span>
            </div>
            <div>
              <p className="text-xs font-semibold text-text-light">Трассировка</p>
              {renderTrace(response.trace)}
            </div>
          </div>
        )}
        <div className="space-y-2">
          <p className="text-xs font-semibold text-text-light">Рабочая память</p>
          {workingMemory.length ? (
            <div className="space-y-2">
              {workingMemory.slice(0, 5).map((slot, index) => (
                <div key={`${slot.q}-${index}`} className="rounded-xl bg-white/60 p-2 text-xs text-text-dark">
                  <p className="font-mono text-sm">q={slot.q}</p>
                  <p>τ={formatNumber(slot.tau)} κ={formatNumber(slot.kappa)} w={formatNumber(slot.weight)}</p>
                  {slot.tags.length > 0 && (
                    <p className="text-[0.65rem] text-text-light">{slot.tags.join(", ")}</p>
                  )}
                </div>
              ))}
            </div>
          ) : (
            <p className="text-xs text-text-light">буфер пуст</p>
          )}
        </div>
      </div>
    </div>
  );
};

export default TracePanel;
