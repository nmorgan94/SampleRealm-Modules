// Mirrors juce::PopupMenu as LookAndFeel_V4 draws it
import { useCallback, useEffect, useLayoutEffect, useRef, useState, type CSSProperties } from 'react';
import { createPortal } from 'react-dom';
import { areaInEditor, useEditorRoot } from './editorRoot.ts';
import { juceHeight } from './fonts.ts';
import type { Rect } from './layout.ts';

export type MenuItem =
  | { kind: 'header'; label: string }
  | { kind: 'separator' }
  | { kind: 'item'; label: string; ticked?: boolean; disabled?: boolean; onSelect?: () => void }
  | { kind: 'submenu'; label: string; ticked?: boolean; items: MenuItem[] };

// getIdealPopupMenuItemSize: the font's height * 1.3
const ITEM_HEIGHT = 17;
const SEPARATOR_HEIGHT = 10;
const BORDER = 2;

// A plugin's stylesheet may give the menu colours of its own: --color-menu, -menu-header, -menu-highlight and -menu-highlight-text
const HIGHLIGHTED = 'bg-[color:var(--color-menu-highlight,var(--color-thumb))] text-[color:var(--color-menu-highlight-text,var(--color-fg))]';
const HOVERED = 'hover:bg-[color:var(--color-menu-highlight,var(--color-thumb))] hover:text-[color:var(--color-menu-highlight-text,var(--color-fg))]';

// Where a list goes, and where it goes instead when there's no room to the right
type Placement = { left: number; top: number; leftIfNoRoom: number };

function Tick() {
  return (
    <svg className="absolute left-0.5 top-1/2 -translate-y-1/2" width={13} height={13} viewBox="0 0 13 13" aria-hidden>
      <path d="M2.5 7 L5.5 10 L10.5 3" stroke="currentColor" strokeWidth={1.6} fill="none" />
    </svg>
  );
}

function Arrow() {
  return (
    <svg className="absolute right-1 top-1/2 -translate-y-1/2" width={6} height={8} viewBox="0 0 6 8" aria-hidden>
      <path d="M0 0 L6 4 L0 8 Z" fill="currentColor" />
    </svg>
  );
}

function MenuList({ items, placement, onClose }: { items: MenuItem[]; placement: Placement; onClose(): void }) {
  const list = useRef<HTMLDivElement>(null);
  const rows = useRef<(HTMLDivElement | null)[]>([]);
  const [box, setBox] = useState<Rect | null>(null);
  const [open, setOpen] = useState<number | null>(null);

  // Kept inside the editor, as JUCE keeps menus on screen
  useLayoutEffect(() => {
    const element = list.current;
    const area = element?.offsetParent as HTMLElement | null;
    if (!element || !area) return;

    const { offsetWidth: width, offsetHeight: height } = element;
    const fitsRight = placement.left + width <= area.clientWidth;
    const left = Math.max(0, fitsRight ? placement.left : placement.leftIfNoRoom - width);
    const top = Math.max(0, Math.min(placement.top, area.clientHeight - height));
    setBox({ left, top, width, height });
  }, [placement.left, placement.top, placement.leftIfNoRoom]);

  const submenu = open === null ? null : items[open];
  const row = open === null ? null : rows.current[open];

  const style: CSSProperties = {
    left: box?.left ?? placement.left,
    top: box?.top ?? placement.top,
    visibility: box ? 'visible' : 'hidden',
    padding: `${BORDER}px 0`,
    fontSize: juceHeight(13),
  };

  return (
    <>
      <div
        ref={list}
        role="menu"
        className="absolute z-41 border border-fg/60 bg-[color:var(--color-menu,var(--color-background))] whitespace-nowrap text-fg shadow-lg"
        style={style}
      >
        {items.map((item, index) => {
          if (item.kind === 'separator') {
            return (
              <div key={index} className="flex items-center px-1" style={{ height: SEPARATOR_HEIGHT }}>
                <div className="h-px w-full bg-fg/30" />
              </div>
            );
          }

          if (item.kind === 'header') {
            return (
              <div key={index} className="flex items-center pr-4 pl-3 font-bold text-[color:var(--color-menu-header,var(--color-fg))]" style={{ height: ITEM_HEIGHT }}>
                {item.label}
              </div>
            );
          }

          const disabled = item.kind === 'item' && item.disabled;
          const highlighted = open === index;

          return (
            <div
              key={index}
              ref={(element) => void (rows.current[index] = element)}
              role="menuitem"
              aria-disabled={disabled}
              aria-haspopup={item.kind === 'submenu'}
              className={`relative mx-px flex items-center rounded-sm pr-5 ${disabled ? 'text-fg/30' : `cursor-pointer ${HOVERED}`} ${highlighted ? HIGHLIGHTED : ''}`}
              style={{ height: ITEM_HEIGHT, paddingLeft: ITEM_HEIGHT }}
              onPointerEnter={(e) => {
                if (e.pointerType !== 'touch') setOpen(item.kind === 'submenu' ? index : null);
              }}
              onClick={() => {
                if (item.kind === 'submenu') setOpen(highlighted ? null : index);
                else if (!disabled) {
                  onClose();
                  item.onSelect?.();
                }
              }}
            >
              {item.ticked && <Tick />}
              {item.label}
              {item.kind === 'submenu' && <Arrow />}
            </div>
          );
        })}
      </div>
      {box && submenu?.kind === 'submenu' && row && (
        <MenuList
          key={open}
          items={submenu.items}
          placement={{ left: box.left + box.width, top: box.top + row.offsetTop - BORDER, leftIfNoRoom: box.left }}
          onClose={onClose}
        />
      )}
    </>
  );
}

interface MenuProps {
  items: MenuItem[];
  // Opens below this
  anchor: Rect;
  onClose(): void;
}

// A click anywhere else closes it, and doesn't reach what's underneath
export function Menu({ items, anchor, onClose }: MenuProps) {
  const root = useEditorRoot();

  // The demo's page around the editor
  useEffect(() => {
    const outside = (e: PointerEvent) => {
      if (!root?.contains(e.target as Node)) onClose();
    };
    const escape = (e: KeyboardEvent) => {
      if (e.key === 'Escape') onClose();
    };

    document.addEventListener('pointerdown', outside);
    document.addEventListener('keydown', escape);
    return () => {
      document.removeEventListener('pointerdown', outside);
      document.removeEventListener('keydown', escape);
    };
  }, [root, onClose]);

  if (!root) return null;

  return createPortal(
    <>
      <div
        className="absolute inset-0 z-40"
        onPointerDown={onClose}
        onContextMenu={(e) => {
          e.preventDefault();
          onClose();
        }}
      />
      <MenuList items={items} placement={{ left: anchor.left, top: anchor.top + anchor.height, leftIfNoRoom: anchor.left + anchor.width }} onClose={onClose} />
    </>,
    root
  );
}

// Opens a menu anchored on an element, or at a point in it; render `element` to show it
export function useMenu() {
  const root = useEditorRoot();
  const [shown, setShown] = useState<{ anchor: Rect; items: MenuItem[] } | null>(null);
  const close = useCallback(() => setShown(null), []);

  const open = (element: HTMLElement, items: MenuItem[], at?: { x: number; y: number }) => {
    if (!root) return;
    const area = areaInEditor(element, root);
    setShown({ anchor: at ? { left: area.left + at.x, top: area.top + at.y, width: 0, height: 0 } : area, items });
  };

  return { open, element: shown && <Menu anchor={shown.anchor} items={shown.items} onClose={close} /> };
}
