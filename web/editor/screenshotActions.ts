// Mirrors HeaderBar::saveScreenshot
import type { EditorBackend } from '../backend.ts';
import type { MenuItem } from './Menu.tsx';
import type { Prompt } from './PromptOverlay.tsx';

export type ScreenshotBackend = Pick<EditorBackend, 'screenshot'>;

// The menu is part of the page, so it is in the picture until the page has drawn without it
const drawnAgain = () => new Promise<void>((resolve) => requestAnimationFrame(() => requestAnimationFrame(() => resolve())));

// Nothing where there is no plugin to save the picture, as in the demo
export function screenshotMenuItems({ screenshot }: ScreenshotBackend, prompt: Prompt, menuGone = drawnAgain): MenuItem[] {
  if (!screenshot) return [];

  const save = async () => {
    await menuGone();
    const error = await screenshot().catch(() => "The screenshot couldn't be taken.");
    if (error) prompt.showMessage('Screenshot', error);
  };

  return [{ kind: 'item', label: 'Screenshot', onSelect: () => void save() }];
}
