// Mirrors editor/ModalOverlay.h
import { useEffect, useRef, type CSSProperties, type KeyboardEvent, type ReactNode, type RefObject } from 'react';
import { juceHeight } from './fonts.ts';

export const TITLE_HEIGHT = 24;

interface ModalProps {
  // Also the title, drawn at titleInset in the panel
  label: string;
  titleInset: { x: number; y: number };
  // The panel's size; it's centred
  panel: CSSProperties;
  onClose(): void;
  // Any key but Escape
  onPanelKey?(e: KeyboardEvent): void;
  // Focused on opening, in place of the panel
  focus?: RefObject<HTMLElement | null>;
  children: ReactNode;
}

// Tab stays inside the overlay, going round its controls
const wrapTab = (e: KeyboardEvent<HTMLElement>) => {
  e.preventDefault();
  const controls = [...e.currentTarget.querySelectorAll<HTMLElement>('a[href], button, input')];
  const index = controls.findIndex((control) => control === e.target);
  const next = index < 0 ? (e.shiftKey ? -1 : 0) : index + (e.shiftKey ? -1 : 1);
  controls.at(next % controls.length)?.focus();
};

export function Modal({ label, titleInset, panel, onClose, onPanelKey, focus, children }: ModalProps) {
  const panelRef = useRef<HTMLDivElement>(null);

  useEffect(() => (focus?.current ?? panelRef.current)?.focus(), [focus]);

  return (
    <div
      tabIndex={-1}
      className="absolute inset-0 z-50 flex items-center justify-center bg-overlay-dim outline-none"
      onPointerDown={(e) => e.stopPropagation()}
      onKeyDown={(e) => {
        // Nothing behind the overlay gets keys
        e.stopPropagation();
        if (e.key === 'Escape') onClose();
        else if (e.key === 'Tab') wrapTab(e);
        else onPanelKey?.(e);
      }}
    >
      <div ref={panelRef} tabIndex={-1} role="dialog" aria-label={label} className="relative rounded-lg bg-overlay-panel outline-none ring-1 ring-inset ring-overlay-outline" style={panel}>
        <div className="absolute flex items-center font-bold" style={{ left: titleInset.x, top: titleInset.y, right: titleInset.x, height: TITLE_HEIGHT, fontSize: juceHeight(14) }}>
          {label}
        </div>
        {children}
      </div>
    </div>
  );
}
