// Mirrors editor/HeaderBar.cpp
import { useEffect, useState, type ReactNode } from 'react';
import { aboutText, type About } from './about.ts';
import type { PresetBackend, UpdateBackend } from '../backend.ts';
import { FittedText, TextButton } from './controls.tsx';
import { juceHeight } from './fonts.ts';
import { centred, type Rect } from './layout.ts';
import { useMenu, type MenuItem } from './Menu.tsx';
import { presetActions } from './presetActions.ts';
import { PresetBar } from './PresetBar.tsx';
import type { Prompt } from './PromptOverlay.tsx';
import { screenshotMenuItems, type ScreenshotBackend } from './screenshotActions.ts';
import { updateMenuItems } from './updateActions.ts';

// HeaderBar::Layout, which each plugin sets
export interface HeaderLayout {
  height: number;
  // Left of the title, right of the meter
  edge: number;
  // A longer title is squeezed to fit
  titleWidth: number;
  titleHeight: number;
  // Either side of the preset bar and menu
  gap: number;
  controlsHeight: number;
  meterWidth: number;
  // Above and below the meter
  meterInset: number;
}

// HeaderBar::resized
const layOut = (width: number, layout: HeaderLayout) => {
  const { height, edge, titleWidth, gap, controlsHeight, meterWidth, meterInset } = layout;
  const meter: Rect = { left: width - edge - meterWidth, top: meterInset, width: meterWidth, height: height - 2 * meterInset };
  const left = edge + titleWidth + gap;
  const controls: Rect = { left, top: centred(height, controlsHeight), width: meter.left - gap - left, height: controlsHeight };
  return {
    title: { left: edge, top: 0, width: titleWidth, height },
    meter,
    menu: { ...controls, left: controls.left + controls.width - 28, width: 28 },
    presetBar: { ...controls, width: controls.width - 28 - 4 },
  };
};

// HeaderBar::menuMarkSize
const MENU_MARK_SIZE = 5;

// HeaderBar::paintOverChildren: centred its own size in from the button's top right corner
const menuMark = (menu: Rect): Rect => ({
  left: menu.left + menu.width - 1.5 * MENU_MARK_SIZE,
  top: menu.top + 0.5 * MENU_MARK_SIZE,
  width: MENU_MARK_SIZE,
  height: MENU_MARK_SIZE,
});

// HeaderBar::Title: bold, then a part in the regular weight
export interface HeaderTitle {
  bold: string;
  regular: string;
}

interface HeaderBarProps {
  // All bold as a string. A function draws a title of the plugin's own in the bounds it's given, its <h1> included
  title: string | HeaderTitle | ((bounds: Rect) => ReactNode);
  width: number;
  layout: HeaderLayout;
  // With `updates`, the menu has their items, and its button a dot while there is a notice: HeaderBar::setMenuMarked
  backend: { info: About; running: string; presets: PresetBackend; updates?: UpdateBackend } & ScreenshotBackend;
  prompt: Prompt;
  showManual(): void;
  meter(bounds: Rect): ReactNode;
  // HeaderBar::setOwnMenuItems: the plugin's own items, asked for each time the menu opens
  ownMenuItems?: () => MenuItem[];
}

export function HeaderBar({ title, width, layout, backend, prompt, showManual, meter, ownMenuItems }: HeaderBarProps) {
  const menu = useMenu();
  const boxes = layOut(width, layout);
  const actions = presetActions(backend.presets, prompt);
  const { updates } = backend;
  const [menuMarked, setMenuMarked] = useState(() => !!updates?.status().notice);

  useEffect(() => {
    setMenuMarked(!!updates?.status().notice);
    return updates?.onStatus((status) => setMenuMarked(!!status.notice));
  }, [updates]);

  const showMenu = async (button: HTMLElement) => {
    menu.open(button, [
      ...(await actions.menuItems()),
      { kind: 'separator' },
      { kind: 'item', label: 'Manual', onSelect: showManual },
      { kind: 'item', label: 'About', onSelect: () => prompt.showMessage('About', aboutText(backend.info, backend.running)) },
      ...screenshotMenuItems(backend, prompt),
      ...(ownMenuItems?.() ?? []),
      ...updateMenuItems(updates),
    ]);
  };

  return (
    <>
      <div className="absolute inset-x-0 top-0 bg-header" style={{ height: layout.height }} />
      {typeof title === 'function' ? (
        title(boxes.title)
      ) : (
        <h1
          className="absolute flex items-center font-bold whitespace-nowrap text-title"
          style={{ ...boxes.title, fontSize: juceHeight(layout.titleHeight) }}
        >
          <FittedText {...(typeof title === 'string' ? { text: title } : { text: title.bold, regular: title.regular })} width={layout.titleWidth} />
        </h1>
      )}
      <PresetBar presets={backend.presets} actions={actions} bounds={boxes.presetBar} />
      <TextButton text="..." style={boxes.menu} onClick={(e) => void showMenu(e.currentTarget)} />
      {menuMarked && <div className="pointer-events-none absolute rounded-full bg-title" style={menuMark(boxes.menu)} />}
      {menu.element}
      {meter(boxes.meter)}
    </>
  );
}
