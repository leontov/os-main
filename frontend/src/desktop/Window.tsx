import React, { useRef, useState } from "react";

const Window: React.FC<{
  id: number;
  title: string;
  onClose: () => void;
  children?: React.ReactNode;
}> = ({ title, onClose, children }) => {
  const ref = useRef<HTMLDivElement | null>(null);
  const [pos, setPos] = useState({ x: 96, y: 96 });
  const [size, setSize] = useState({ w: 384, h: 320 });
  const dragRef = useRef<{ sx: number; sy: number; ox: number; oy: number } | null>(null);
  const resizeRef = useRef<{ sw: number; sh: number; ox: number; oy: number } | null>(null);

  const onPointerDown = (e: React.PointerEvent) => {
    const p = e.currentTarget.getBoundingClientRect();
    (e.currentTarget as Element).setPointerCapture(e.pointerId);
    dragRef.current = {
      sx: e.clientX,
      sy: e.clientY,
      ox: pos.x,
      oy: pos.y,
    };
  };

  const onPointerMove = (e: React.PointerEvent) => {
    if (dragRef.current) {
      const d = dragRef.current;
      const nx = d.ox + (e.clientX - d.sx);
      const ny = d.oy + (e.clientY - d.sy);
      setPos({ x: Math.max(8, nx), y: Math.max(8, ny) });
    }
    if (resizeRef.current) {
      const r = resizeRef.current;
      const nw = Math.max(200, r.sw + (e.clientX - r.ox));
      const nh = Math.max(120, r.sh + (e.clientY - r.oy));
      setSize({ w: nw, h: nh });
    }
  };

  const onPointerUp = (e: React.PointerEvent) => {
    dragRef.current = null;
    resizeRef.current = null;
    try {
      (e.currentTarget as Element).releasePointerCapture(e.pointerId);
    } catch {}
  };

  const onResizePointerDown = (e: React.PointerEvent) => {
    e.stopPropagation();
    (e.currentTarget as Element).setPointerCapture(e.pointerId);
    resizeRef.current = { sw: size.w, sh: size.h, ox: e.clientX, oy: e.clientY };
  };

  return (
    <div
      ref={ref}
      className="absolute bg-white text-black rounded shadow-lg overflow-hidden"
      style={{ left: pos.x, top: pos.y, width: size.w, height: size.h }}
      onPointerMove={onPointerMove}
      onPointerUp={onPointerUp}
      onPointerCancel={onPointerUp}
    >
      <div
        className="bg-slate-900 text-white px-3 py-2 flex items-center justify-between cursor-grab select-none"
        onPointerDown={onPointerDown}
      >
        <div className="font-semibold">{title}</div>
        <div>
          <button className="px-2" onClick={onClose}>
            ✕
          </button>
        </div>
      </div>
      <div className="p-2 overflow-auto h-full" style={{ height: size.h - 48 }}>{children}</div>
      <div
        className="absolute right-0 bottom-0 w-4 h-4 cursor-se-resize bg-transparent"
        onPointerDown={onResizePointerDown}
      />
    </div>
  );
};

export default Window;
