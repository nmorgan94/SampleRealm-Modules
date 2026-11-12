// Mirrors juce::TextButton and drawFittedText as LookAndFeel_V4 draws them
import { useLayoutEffect, useRef, useState, type CSSProperties, type MouseEvent as ReactMouseEvent } from 'react';
import { juceHeight } from './fonts.ts';

const MIN_SQUEEZE = 0.7;

// As drawFittedText on one line: squeezed to fit, as far as 70% wide, then cut short with "…".
// `regular` follows the text in the regular weight
export function FittedText({ text, regular = '', width }: { text: string; regular?: string; width: number }) {
  const span = useRef<HTMLSpanElement>(null);
  const [squeeze, setSqueeze] = useState(1);

  useLayoutEffect(() => {
    const measure = () => {
      const natural = span.current?.scrollWidth ?? 0;
      setSqueeze(natural > width ? Math.max(MIN_SQUEEZE, width / natural) : 1);
    };
    measure();
    document.fonts?.ready.then(measure);
  }, [text, regular, width]);

  return (
    <span
      ref={span}
      className="inline-block shrink-0 origin-left overflow-hidden text-ellipsis whitespace-pre"
      style={{ maxWidth: width / squeeze, transform: `scaleX(${squeeze})` }}
    >
      {text}
      {regular && <span className="font-normal">{regular}</span>}
    </span>
  );
}

interface TextButtonProps {
  text: string;
  style: CSSProperties;
  onClick(e: ReactMouseEvent<HTMLButtonElement>): void;
}

// LookAndFeel_V4::drawButtonBackground lightens by 5% on hover and 20% while down
export function TextButton({ text, style, onClick }: TextButtonProps) {
  return (
    <button
      type="button"
      className="absolute cursor-pointer overflow-hidden rounded-md border border-line bg-widget px-1 text-ellipsis whitespace-nowrap text-[color:var(--color-button-text,var(--color-fg))] hover:bg-[color-mix(in_srgb,var(--color-widget),white_5%)] active:bg-[color-mix(in_srgb,var(--color-widget),white_20%)]"
      style={{ fontSize: juceHeight(12), ...style }}
      onClick={onClick}
    >
      {text}
    </button>
  );
}
