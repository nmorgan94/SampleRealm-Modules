import { createContext, useContext } from 'react';
import type { Rect } from './layout.ts';

// Where popups go, so they're positioned against the editor wherever they're opened from
export const EditorRootContext = createContext<HTMLElement | null>(null);

export const useEditorRoot = () => useContext(EditorRootContext);

// In the editor's own pixels, which the demo's scaling doesn't change
export function areaInEditor(element: HTMLElement, root: HTMLElement): Rect {
  let left = 0;
  let top = 0;
  for (let node: HTMLElement | null = element; node && node !== root; node = node.offsetParent as HTMLElement | null) {
    left += node.offsetLeft;
    top += node.offsetTop;
  }
  return { left, top, width: element.offsetWidth, height: element.offsetHeight };
}
