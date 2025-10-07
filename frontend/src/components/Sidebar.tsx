import {
  BarChart3,
  Bot,
  Clock3,
  MessageCircle,
  Settings,
  Sparkles,
} from "lucide-react";
import NavItem from "./NavItem";
import TracePanel from "./TracePanel";

const Sidebar = () => (
  <div className="flex h-full flex-col justify-between rounded-3xl bg-background-sidebar/70 p-6 backdrop-blur-xl">
    <div className="space-y-8">
      <div className="flex items-center gap-3">
        <div className="flex h-12 w-12 items-center justify-center rounded-2xl bg-white shadow-card">
          <img src="/kolibri.svg" alt="Колибри" className="h-10 w-10" />
        </div>
        <div>
          <p className="text-sm font-medium text-text-light">Колибри ИИ</p>
          <p className="text-lg font-semibold text-text-dark">Визуальная Кора</p>
        </div>
      </div>
      <nav className="space-y-2">
        <NavItem icon={MessageCircle} label="Диалоги" active />
        <NavItem icon={Sparkles} label="Действия" />
        <NavItem icon={BarChart3} label="Визуализация" />
        <NavItem icon={Clock3} label="История" />
        <NavItem icon={Settings} label="Настройки" />
      </nav>
      <TracePanel />
    </div>
    <div className="flex items-center gap-3 rounded-2xl bg-white/70 p-3">
      <div className="flex h-12 w-12 items-center justify-center rounded-xl bg-primary/10 text-primary">
        <Bot className="h-6 w-6" />
      </div>
      <div>
        <p className="text-sm font-semibold text-text-dark">Vladislav Kochurov</p>
        <p className="text-xs text-text-light">Колибри может делать ошибки.</p>
      </div>
    </div>
  </div>
);

export default Sidebar;
