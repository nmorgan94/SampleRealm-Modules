// Mirrors juce::TextButton and drawFittedText as LookAndFeel_V4 draws them
import { useLayoutEffect, useRef, useState, type CSSProperties, type MouseEvent as ReactMouseEvent } from 'react';
import { juceHeight } from './fonts.ts';

const MIN_SQUEEZE = 0.7;

// As drawFittedText on one line: squeezed to fit, as far as 70% wide, then cut short with "…".
// `regular` follows the text in the regular weight
export function FittedText({ text, regular = '', width, centred = false }: { text: string; regular?: string; width: number; centred?: boolean }) {
  const span = useRef<HTMLSpanElement>(null);
  const [natural, setNatural] = useState(0);
  const room = width / MIN_SQUEEZE;
  const squeeze = natural > width ? Math.max(MIN_SQUEEZE, width / natural) : 1;

  useLayoutEffect(() => {
    const measure = () => setNatural(span.current?.scrollWidth ?? 0);
    measure();
    document.fonts?.ready.then(measure);
  }, [text, regular]);

  return (
    <span
      ref={span}
      className={`inline-block shrink-0 ${centred ? 'origin-center' : 'origin-left'} overflow-hidden text-ellipsis whitespace-pre`}
      style={{ maxWidth: natural > room ? room : undefined, transform: `scaleX(${squeeze})` }}
    >
      {text}
      {regular && <span className="font-normal">{regular}</span>}
    </span>
  );
}

interface TextButtonProps {
  text: string;
  style: CSSProperties;
  // For screen readers, where the text alone doesn't say what the button is for: as Component::setTitle
  label?: string;
  onClick(e: ReactMouseEvent<HTMLButtonElement>): void;
}

// LookAndFeel_V4::drawButtonBackground lightens by 5% on hover and 20% while down
export function TextButton({ text, style, label, onClick }: TextButtonProps) {
  return (
    <button
      type="button"
      className="absolute cursor-pointer overflow-hidden rounded-md border border-line bg-widget px-1 text-ellipsis whitespace-nowrap text-[color:var(--color-button-text,var(--color-fg))] hover:bg-[color-mix(in_srgb,var(--color-widget),white_5%)] active:bg-[color-mix(in_srgb,var(--color-widget),white_20%)]"
      style={{ fontSize: juceHeight(12), ...style }}
      aria-label={label}
      onClick={onClick}
    >
      {text}
    </button>
  );
}
