/**
 * LogoText Component - Premium metallic logo with cursor-following highlight
 * Creates a realistic metallic sheen effect that follows cursor position
 */

import { useRef, useCallback, useState, useEffect } from 'react';

interface LogoTextProps {
  text: string;
  disabled?: boolean;
}

export function LogoText({ text, disabled = false }: LogoTextProps) {
  const containerRef = useRef<HTMLDivElement>(null);
  const [mousePos, setMousePos] = useState<{ x: number; y: number } | null>(null);
  const [isHovering, setIsHovering] = useState(false);

  const handleMouseMove = useCallback((e: React.MouseEvent) => {
    if (disabled || !containerRef.current) return;

    const rect = containerRef.current.getBoundingClientRect();
    // Calculate position as percentage (0-100) relative to the element
    const x = ((e.clientX - rect.left) / rect.width) * 100;
    const y = ((e.clientY - rect.top) / rect.height) * 100;

    setMousePos({ x, y });
  }, [disabled]);

  const handleMouseEnter = useCallback(() => {
    if (!disabled) {
      setIsHovering(true);
    }
  }, [disabled]);

  const handleMouseLeave = useCallback(() => {
    setIsHovering(false);
    setMousePos(null);
  }, []);

  // Disable effect when disabled prop changes to true while hovering
  useEffect(() => {
    if (disabled && isHovering) {
      setIsHovering(false);
      setMousePos(null);
    }
  }, [disabled, isHovering]);

  // Calculate the highlight style based on mouse position
  const getHighlightStyle = () => {
    if (!isHovering || !mousePos || disabled) {
      return {};
    }

    // Create a dynamic radial gradient that follows the cursor
    // The highlight is a warm golden sheen like light hitting brushed gold
    // Tight, focused spotlight effect
    const highlightGradient = `
      radial-gradient(
        ellipse 100px 70px at ${mousePos.x}% ${mousePos.y}%,
        rgba(255, 250, 220, 1) 0%,
        rgba(255, 230, 170, 0.85) 20%,
        rgba(255, 210, 130, 0.5) 40%,
        rgba(255, 190, 100, 0.2) 60%,
        transparent 75%
      )
    `;

    return {
      '--highlight-gradient': highlightGradient,
      '--highlight-opacity': '1',
    } as React.CSSProperties;
  };

  return (
    <div
      ref={containerRef}
      className={`logo-text-container ${isHovering && !disabled ? 'logo-hovering' : ''}`}
      onMouseMove={handleMouseMove}
      onMouseEnter={handleMouseEnter}
      onMouseLeave={handleMouseLeave}
      style={getHighlightStyle()}
    >
      <span className="logo-text-base">
        {text}
      </span>
      <span className="logo-text-highlight" aria-hidden="true">
        {text}
      </span>
    </div>
  );
}
