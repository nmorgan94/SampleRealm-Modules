import { useEffect, useRef, useState, type ReactNode } from 'react';

// The site's page sizes its frame from this
const SIZE_MESSAGE = 'samplerealm-demo-size';

type DemoStatus = 'loading' | 'ready' | 'failed';

interface Props {
  // The editor's size, unscaled
  width: number;
  height: number;
  status: DemoStatus;
  editor: ReactNode;
  // What the page adds under the editor: its Play button or keyboard
  children?: ReactNode;
}

// The editor, scaled down to fit a narrow frame. Its colours are the plugin's tokens: background, clip
export function DemoFrame({ width, height, status, editor, children }: Props) {
  const [scale, setScale] = useState(1);
  const content = useRef<HTMLDivElement>(null);
  const editorBox = useRef<HTMLDivElement>(null);

  useEffect(() => {
    const element = content.current;
    if (!element) return;
    const observer = new ResizeObserver(() => setScale(Math.min(1, element.clientWidth / width)));
    observer.observe(element);
    return () => observer.disconnect();
  }, [width]);

  // Not the document's height, which never shrinks below the frame. The editor's box is measured,
  // as its scale catches up with the frame's width a render late
  useEffect(() => {
    const element = content.current;
    const box = editorBox.current;
    if (window.parent === window || !element || !box) return;

    const report = () => {
      const below = Math.round(element.getBoundingClientRect().height - box.getBoundingClientRect().height);
      window.parent.postMessage({ type: SIZE_MESSAGE, editorWidth: width, editorHeight: height, below }, '*');
    };
    const observer = new ResizeObserver(report);
    observer.observe(element);
    return () => observer.disconnect();
  }, [width, height]);

  return (
    <div className="mx-auto" style={{ maxWidth: width }} ref={content}>
      <div className="relative overflow-hidden rounded-xl" ref={editorBox} style={{ width: width * scale, height: height * scale }}>
        <div className="origin-top-left" style={{ width, height, transform: `scale(${scale})` }}>
          {editor}
        </div>
        {status !== 'ready' && (
          <div className="absolute inset-0 z-30 flex items-center justify-center bg-background/55">
            {status === 'failed' ? (
              <p className="text-sm text-clip">The demo couldn't start. Please try again later.</p>
            ) : (
              <p className="text-sm font-bold">Loading…</p>
            )}
          </div>
        )}
      </div>

      {children}
    </div>
  );
}
