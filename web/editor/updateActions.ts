// Mirrors updates/UpdateActions.cpp
import type { UpdateBackend } from '../backend.ts';
import type { MenuItem } from './Menu.tsx';

// The notice when there is one, and the switch. Nothing in a plugin that doesn't check
export function updateMenuItems(updates: UpdateBackend | undefined): MenuItem[] {
  if (!updates) return [];

  const { notice, checking } = updates.status();

  return [
    { kind: 'separator' },
    ...(notice ? [{ kind: 'item' as const, label: notice, onSelect: () => updates.openPage() }] : []),
    { kind: 'item', label: 'Check for updates', ticked: checking, onSelect: () => updates.setChecking(!checking) },
  ];
}
