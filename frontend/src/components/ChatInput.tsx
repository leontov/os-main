import { Paperclip, Plus, RefreshCw, SendHorizontal } from "lucide-react";
import { useId } from "react";

interface ChatInputProps {
  value: string;
  mode: string;
  isBusy: boolean;
  onChange: (value: string) => void;
  onModeChange: (mode: string) => void;
  onSubmit: () => void;
  onReset: () => void;
}

const modes = ["Быстрый ответ", "Исследование", "Творческий"];

const ChatInput = ({ value, mode, isBusy, onChange, onModeChange, onSubmit, onReset }: ChatInputProps) => {
  const textAreaId = useId();

  return (
    <div className="mt-6 flex flex-col gap-4 rounded-3xl bg-white/80 p-6 shadow-card">
      <div className="flex items-center gap-3 text-sm text-text-light">
        <div className="flex h-10 w-10 items-center justify-center rounded-xl bg-accent-coral/10 text-accent-coral">
          К
        </div>
        <div className="flex items-center gap-3">
          <label htmlFor={textAreaId} className="text-text-dark">
            Режим
          </label>
          <select
            id={textAreaId}
            className="rounded-xl border border-transparent bg-background-light/60 px-3 py-2 text-sm font-medium text-text-dark focus:border-primary focus:outline-none"
            value={mode}
            onChange={(event) => onModeChange(event.target.value)}
            disabled={isBusy}
          >
            {modes.map((item) => (
              <option key={item} value={item}>
                {item}
              </option>
            ))}
          </select>
        </div>
      </div>
      <textarea
        id={`${textAreaId}-textarea`}
        value={value}
        onChange={(event) => onChange(event.target.value)}
        placeholder="Сообщение для Колибри"
        className="min-h-[120px] w-full resize-none rounded-2xl border border-transparent bg-background-light/60 px-4 py-3 text-sm text-text-dark placeholder:text-text-light focus:border-primary focus:outline-none"
      />
      <div className="flex flex-wrap items-center justify-between gap-3">
        <div className="flex gap-2 text-text-light">
          <button
            type="button"
            className="flex items-center gap-2 rounded-xl bg-background-light/60 px-3 py-2 text-xs font-semibold text-text-light transition-colors hover:text-text-dark"
            disabled={isBusy}
          >
            <Paperclip className="h-4 w-4" />
            Вложить
          </button>
          <button
            type="button"
            onClick={onReset}
            className="flex items-center gap-2 rounded-xl bg-background-light/60 px-3 py-2 text-xs font-semibold text-text-light transition-colors hover:text-text-dark"
          >
            <Plus className="h-4 w-4" />
            Новый диалог
          </button>
        </div>
        <div className="flex gap-2">
          <button
            type="button"
            onClick={onReset}
            className="flex items-center gap-2 rounded-xl bg-background-light/60 px-4 py-2 text-sm font-medium text-text-light transition-colors hover:text-text-dark"
            disabled={isBusy}
          >
            <RefreshCw className="h-4 w-4" />
            Сбросить
          </button>
          <button
            type="button"
            onClick={onSubmit}
            className="flex items-center gap-2 rounded-xl bg-primary px-4 py-2 text-sm font-semibold text-white transition-opacity hover:opacity-90 disabled:cursor-not-allowed disabled:opacity-60"
            disabled={isBusy || !value.trim()}
          >
            <SendHorizontal className="h-4 w-4" />
            Отправить
          </button>
        </div>
      </div>
    </div>
  );
};

export default ChatInput;
