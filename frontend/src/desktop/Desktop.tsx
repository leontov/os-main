import React, { useState } from "react";
import Taskbar from "./Taskbar";
import Window from "./Window";

const Desktop: React.FC = () => {
  const [windows, setWindows] = useState<Array<{ id: number; title: string; content: React.ReactNode }>>([
    { id: 1, title: "Welcome", content: <div className="p-4">Welcome to Kolibri Desktop</div> },
  ]);

  const closeWindow = (id: number) => setWindows((w) => w.filter((x) => x.id !== id));

  return (
    <div className="w-full h-full bg-slate-900 text-white">
      <div className="p-4">
        <h1 className="text-2xl font-bold">Kolibri Desktop (experimental)</h1>
      </div>
      <div className="p-4">
        <div className="grid grid-cols-3 gap-4">
          <div className="bg-slate-700 rounded p-4">Widget 1</div>
          <div className="bg-slate-700 rounded p-4">Widget 2</div>
          <div className="bg-slate-700 rounded p-4">Widget 3</div>
        </div>
      </div>

      {windows.map((w) => (
        <Window key={w.id} id={w.id} title={w.title} onClose={() => closeWindow(w.id)}>
          {w.content}
        </Window>
      ))}

      <Taskbar onLaunch={(title, content) => setWindows((s) => [...s, { id: Date.now(), title, content }])} />
    </div>
  );
};

export default Desktop;
