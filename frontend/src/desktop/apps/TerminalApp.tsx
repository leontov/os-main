import React, { useState, useEffect } from "react";
import kolibriBridge from "../../core/kolibri-bridge";

const TerminalApp: React.FC = () => {
  const [lines, setLines] = useState<string[]>(["Kolibri Terminal v0.1", ""]);
  const [input, setInput] = useState("");

  const [ready, setReady] = useState(false);

  useEffect(() => {
    void kolibriBridge.ready.then(() => setReady(true)).catch(() => setReady(false));
  }, []);

  const handleSubmit = async (e?: React.FormEvent) => {
    e?.preventDefault();
    if (!input.trim()) return;
    const cmd = input.trim();
    setLines((l) => [...l, `$ ${cmd}`]);
    setInput("");
    try {
      const out = await kolibriBridge.ask(cmd);
      setLines((l) => [...l, ...out.split(/\r?\n/)]);
    } catch (err) {
      setLines((l) => [...l, `Error: ${(err instanceof Error && err.message) || String(err)}`]);
    }
  };

  return (
    <div className="font-mono text-sm text-slate-900">
      <div className="h-56 overflow-auto bg-black text-green-400 p-2 rounded">{lines.map((ln, i) => <div key={i}>{ln}</div>)}</div>
      <form className="mt-2 flex" onSubmit={handleSubmit}>
        <input
          className="flex-1 px-2 py-1 border rounded-l"
          value={input}
          onChange={(e) => setInput(e.target.value)}
          placeholder="$ echo hello"
        />
        <button className="bg-slate-700 text-white px-3 rounded-r" onClick={handleSubmit}>Run</button>
      </form>
    </div>
  );
};

export default TerminalApp;
