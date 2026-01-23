/**
 * Sparks Component - Click-triggered particle effects
 * Creates small sparks that fly out from click position
 */

import { useState, useCallback, useRef } from 'react';
import { motion, AnimatePresence } from 'framer-motion';

interface Spark {
  id: number;
  x: number;
  y: number;
  angle: number;
  distance: number;
  size: number;
  duration: number;
}

export type SparkColor = 'metal' | 'orange' | 'gold';

interface SparkColors {
  primary: string;
  secondary: string;
  glow: string;
}

const SPARK_COLORS: Record<SparkColor, SparkColors> = {
  metal: {
    primary: '#ffffff',
    secondary: '#a0b0c0',
    glow: 'rgba(200, 220, 255, 0.8)',
  },
  orange: {
    primary: '#ff8c00',
    secondary: '#ff6600',
    glow: 'rgba(255, 140, 0, 0.8)',
  },
  gold: {
    primary: '#ffd700',
    secondary: '#ffb800',
    glow: 'rgba(255, 215, 0, 0.8)',
  },
};

interface SparksProps {
  color: SparkColor;
  /** Number of sparks per click */
  count?: number;
  /** Whether sparks are enabled */
  enabled?: boolean;
  children: React.ReactNode;
  className?: string;
  style?: React.CSSProperties;
  onClick?: (e: React.MouseEvent) => void;
}

export function Sparks({
  color,
  count = 8,
  enabled = true,
  children,
  className,
  style,
  onClick,
}: SparksProps) {
  const [sparks, setSparks] = useState<Spark[]>([]);
  const sparkIdRef = useRef(0);
  const containerRef = useRef<HTMLDivElement>(null);

  const createSparks = useCallback((e: React.MouseEvent) => {
    if (!enabled || !containerRef.current) return;

    const rect = containerRef.current.getBoundingClientRect();
    const x = e.clientX - rect.left;
    const y = e.clientY - rect.top;

    const newSparks: Spark[] = [];
    for (let i = 0; i < count; i++) {
      newSparks.push({
        id: sparkIdRef.current++,
        x,
        y,
        angle: Math.random() * Math.PI * 2,
        distance: 30 + Math.random() * 50,
        size: 2 + Math.random() * 3,
        duration: 0.3 + Math.random() * 0.3,
      });
    }

    setSparks(prev => [...prev, ...newSparks]);

    // Clean up old sparks
    setTimeout(() => {
      setSparks(prev => prev.filter(s => !newSparks.includes(s)));
    }, 800);
  }, [enabled, count]);

  const handleClick = useCallback((e: React.MouseEvent) => {
    createSparks(e);
    onClick?.(e);
  }, [createSparks, onClick]);

  const colors = SPARK_COLORS[color];

  return (
    <div
      ref={containerRef}
      className={className}
      style={{ position: 'relative', ...style }}
      onClick={handleClick}
    >
      {children}

      {/* Spark particles */}
      <AnimatePresence>
        {sparks.map(spark => {
          const endX = spark.x + Math.cos(spark.angle) * spark.distance;
          const endY = spark.y + Math.sin(spark.angle) * spark.distance;
          const useSecondary = Math.random() > 0.5;

          return (
            <motion.div
              key={spark.id}
              initial={{
                x: spark.x,
                y: spark.y,
                scale: 1,
                opacity: 1,
              }}
              animate={{
                x: endX,
                y: endY,
                scale: 0,
                opacity: 0,
              }}
              exit={{ opacity: 0 }}
              transition={{
                duration: spark.duration,
                ease: 'easeOut',
              }}
              style={{
                position: 'absolute',
                width: spark.size,
                height: spark.size,
                borderRadius: '50%',
                backgroundColor: useSecondary ? colors.secondary : colors.primary,
                boxShadow: `0 0 ${spark.size * 2}px ${colors.glow}`,
                pointerEvents: 'none',
                zIndex: 1000,
                transform: 'translate(-50%, -50%)',
              }}
            />
          );
        })}
      </AnimatePresence>
    </div>
  );
}

/**
 * Hook for adding sparks to any element
 */
export function useSparks(color: SparkColor, count = 8) {
  const [sparks, setSparks] = useState<Spark[]>([]);
  const sparkIdRef = useRef(0);

  const triggerSparks = useCallback((x: number, y: number) => {
    const newSparks: Spark[] = [];
    for (let i = 0; i < count; i++) {
      newSparks.push({
        id: sparkIdRef.current++,
        x,
        y,
        angle: Math.random() * Math.PI * 2,
        distance: 30 + Math.random() * 50,
        size: 2 + Math.random() * 3,
        duration: 0.3 + Math.random() * 0.3,
      });
    }

    setSparks(prev => [...prev, ...newSparks]);

    setTimeout(() => {
      setSparks(prev => prev.filter(s => !newSparks.includes(s)));
    }, 800);
  }, [count]);

  const colors = SPARK_COLORS[color];

  const SparkElements = (
    <AnimatePresence>
      {sparks.map(spark => {
        const endX = Math.cos(spark.angle) * spark.distance;
        const endY = Math.sin(spark.angle) * spark.distance;
        const useSecondary = Math.random() > 0.5;

        return (
          <motion.div
            key={spark.id}
            initial={{
              x: 0,
              y: 0,
              scale: 1,
              opacity: 1,
            }}
            animate={{
              x: endX,
              y: endY,
              scale: 0,
              opacity: 0,
            }}
            exit={{ opacity: 0 }}
            transition={{
              duration: spark.duration,
              ease: 'easeOut',
            }}
            style={{
              position: 'absolute',
              left: spark.x - spark.size / 2,
              top: spark.y - spark.size / 2,
              width: spark.size,
              height: spark.size,
              borderRadius: '50%',
              backgroundColor: useSecondary ? colors.secondary : colors.primary,
              boxShadow: `0 0 ${spark.size * 2}px ${colors.glow}`,
              pointerEvents: 'none',
              zIndex: 1000,
            }}
          />
        );
      })}
    </AnimatePresence>
  );

  return { triggerSparks, SparkElements };
}
