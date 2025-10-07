import {
  BookOpenText,
  BrainCircuit,
  Compass,
  Lightbulb,
  Rocket,
  Wand2,
} from "lucide-react";
import SuggestionCard from "./SuggestionCard";

const suggestionItems = [
  {
    icon: Rocket,
    title: "Перо в небе",
    description: "Начнём писать!",
    prompt: "Помоги мне написать текст...",
  },
  {
    icon: Compass,
    title: "Соберём план",
    description: "Соберём маршрут идей.",
    prompt: "Составь подробный план по теме...",
  },
  {
    icon: BookOpenText,
    title: "Прочитать тексты",
    description: "Напоминание изучаться.",
    prompt: "Какие ключевые материалы стоит прочитать по теме...",
  },
  {
    icon: BrainCircuit,
    title: "Откроем горизонты",
    description: "Загрузим знания!",
    prompt: "Подскажи новые горизонты знаний для...",
  },
  {
    icon: Lightbulb,
    title: "Переведи мысли в варианты",
    description: "Варианты решений и идей.",
    prompt: "Помоги придумать несколько вариантов...",
  },
  {
    icon: Wand2,
    title: "Вплетём метафоры",
    description: "Украсим смысл.",
    prompt: "Подбери выразительные метафоры для...",
  },
] as const;

interface WelcomeScreenProps {
  onSuggestionSelect: (prompt: string) => void;
}

const WelcomeScreen = ({ onSuggestionSelect }: WelcomeScreenProps) => (
  <section className="flex h-full flex-col justify-between rounded-3xl bg-white/70 p-10 shadow-card">
    <div>
      <p className="text-sm text-text-light">Привет, Владислав!</p>
      <h1 className="mt-2 text-3xl font-semibold text-text-dark">
        Я — Колибри ИИ. Готов взлететь вместе с твоими идеями. Что создадим сегодня?
      </h1>
      <p className="mt-4 text-sm text-text-light">Выбери подсказку или начни писать свой запрос.</p>
    </div>
    <div className="grid grid-cols-1 gap-4 pt-10 md:grid-cols-2 xl:grid-cols-3">
      {suggestionItems.map((item) => (
        <SuggestionCard
          key={item.title}
          icon={item.icon}
          title={item.title}
          description={item.description}
          onSelect={() => onSuggestionSelect(item.prompt)}
        />
      ))}
    </div>
  </section>
);

export default WelcomeScreen;
