import type { ElementType } from "react";

interface SuggestionCardProps {
  icon: ElementType;
  title: string;
  description: string;
  onSelect: () => void;
}

const SuggestionCard = ({ icon: Icon, title, description, onSelect }: SuggestionCardProps) => (
  <button
    type="button"
    onClick={onSelect}
    className="flex flex-col gap-3 rounded-2xl bg-white/80 p-5 text-left shadow-card transition-transform hover:-translate-y-1 hover:shadow-lg"
  >
    <div className="flex h-10 w-10 items-center justify-center rounded-xl bg-primary/10 text-primary">
      <Icon className="h-5 w-5" />
    </div>
    <div>
      <p className="text-base font-semibold text-text-dark">{title}</p>
      <p className="mt-1 text-sm text-text-light">{description}</p>
    </div>
  </button>
);

export default SuggestionCard;
