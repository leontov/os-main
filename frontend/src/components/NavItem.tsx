import type { ElementType } from "react";
import { twMerge } from "tailwind-merge";

interface NavItemProps {
  icon: ElementType;
  label: string;
  active?: boolean;
}

const NavItem = ({ icon: Icon, label, active = false }: NavItemProps) => (
  <button
    type="button"
    className={twMerge(
      "flex w-full items-center gap-3 rounded-xl px-4 py-3 text-left text-sm font-medium transition-colors",
      active
        ? "bg-white/90 text-text-dark shadow-sm"
        : "text-text-light hover:bg-white/70 hover:text-text-dark",
    )}
  >
    <Icon className={twMerge("h-5 w-5", active ? "text-primary" : "text-text-light")} />
    <span>{label}</span>
  </button>
);

export default NavItem;
