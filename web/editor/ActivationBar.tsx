// Mirrors editor/ActivationBar.cpp
import { useSyncExternalStore } from 'react';
import type { ActivationBackend } from '../backend.ts';
import { TextButton } from './controls.tsx';
import { juceHeight } from './fonts.ts';
import type { Rect } from './layout.ts';
import type { Prompt } from './PromptOverlay.tsx';

// ActivationBar::height
export const ACTIVATION_BAR_HEIGHT = 36;

// ActivationBar::resized
function layOut(width: number) {
  const top = 6;
  const height = ACTIVATION_BAR_HEIGHT - 2 * top;
  const at = (left: number, boxWidth: number): Rect => ({ left, top, width: boxWidth, height });
  const importFile = at(width - 12 - 62, 62);
  const paste = at(importFile.left - 6 - 62, 62);
  const activate = at(paste.left - 6 - 84, 84);
  return { importFile, paste, activate, machine: at(12, activate.left - 12 - 12) };
}

interface ActivationBarProps {
  activation: ActivationBackend;
  width: number;
  prompt: Prompt;
}

export function ActivationBar({ activation, width, prompt }: ActivationBarProps) {
  const status = useSyncExternalStore(activation.onStatus, activation.status);
  const boxes = layOut(width);

  if (!status.show) return null;

  const finish = async (said: Promise<string | null>) => {
    const message = await said;
    if (message) prompt.showMessage('Activate', message);
  };

  return (
    <section
      aria-label="Activation"
      className="absolute inset-x-0 bottom-0 bg-activation"
      style={{ height: ACTIVATION_BAR_HEIGHT }}
    >
      <div className="absolute inset-x-0 top-0 h-px bg-activation-outline" />
      <div className="absolute flex items-center whitespace-nowrap" style={{ ...boxes.machine, fontSize: juceHeight(15) }}>
        {status.machine}
      </div>
      <TextButton text="Activate" style={boxes.activate} onClick={activation.openBrowser} />
      <TextButton text="Paste" style={boxes.paste} onClick={() => void finish(activation.paste())} />
      <TextButton text="Import" style={boxes.importFile} onClick={() => void finish(activation.importFile())} />
    </section>
  );
}
