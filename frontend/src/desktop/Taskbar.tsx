import React from "react";
import TerminalApp from "./apps/TerminalApp";

const Taskbar: React.FC<{ onLaunch: (title: string, content: React.ReactNode) => void }> = ({ onLaunch }) => {
  return (
    <div className="fixed bottom-0 left-0 right-0 h-12 bg-slate-800 flex items-center px-4 space-x-4">
      <button
        className="bg-slate-600 px-3 py-1 rounded"
        onClick={() => onLaunch("Terminal", <TerminalApp />)}
      >
        Terminal
      </button>
      <button
        className="bg-slate-600 px-3 py-1 rounded"
        onClick={() => onLaunch("Files", <div className="p-4">Files app (mock)</div>)}
      >
        Files
      </button>
      <div className="ml-auto text-sm text-slate-300">Kolibri Desktop</div>
    </div>
  );
};

export default Taskbar;
