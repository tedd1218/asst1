#!/usr/bin/env python3
"""
Generate graphs from light scaling comparison results.

Usage:
    python3 generate_light_scaling_graphs.py <csv_file> [output_dir]

Example:
    python3 generate_light_scaling_graphs.py output/light_scaling_comparison.csv output/
"""

import sys
import os
import csv

try:
    import matplotlib.pyplot as plt
    import numpy as np
    HAS_MATPLOTLIB = True
except ImportError:
    HAS_MATPLOTLIB = False
    print("Warning: matplotlib not available. Install with: pip install matplotlib")

def load_csv(csv_path):
    """Load the light scaling comparison CSV."""
    data = {
        'lights': [],
        'forward_fps': [],
        'forward_ms': [],
        'deferred_fps': [],
        'deferred_ms': [],
        'winner': [],
        'speedup': []
    }
    
    with open(csv_path, 'r') as f:
        reader = csv.DictReader(f)
        for row in reader:
            data['lights'].append(int(row['LightCount']))
            data['forward_fps'].append(float(row['ForwardFPS']))
            data['forward_ms'].append(float(row['ForwardTimeMs']))
            data['deferred_fps'].append(float(row['DeferredFPS']))
            data['deferred_ms'].append(float(row['DeferredTimeMs']))
            data['winner'].append(row['Winner'])
            data['speedup'].append(float(row['Speedup']))
    
    return data

def find_crossover(data):
    """Find the approximate crossover point."""
    for i, speedup in enumerate(data['speedup']):
        if speedup > 1.0:
            return data['lights'][i]
    return None

def generate_fps_comparison_graph(data, output_path):
    """Generate FPS comparison graph."""
    plt.figure(figsize=(10, 6))
    
    lights = data['lights']
    
    plt.plot(lights, data['forward_fps'], 'b-o', linewidth=2, markersize=8, label='Forward Renderer')
    plt.plot(lights, data['deferred_fps'], 'r-s', linewidth=2, markersize=8, label='Deferred Renderer')
    
    # Mark crossover point
    crossover = find_crossover(data)
    if crossover:
        plt.axvline(x=crossover, color='green', linestyle='--', alpha=0.7, label=f'Crossover (~{crossover} lights)')
    
    plt.xlabel('Number of Lights', fontsize=12)
    plt.ylabel('Frames Per Second (FPS)', fontsize=12)
    plt.title('Forward vs Deferred Rendering: FPS by Light Count', fontsize=14)
    plt.legend(loc='best', fontsize=10)
    plt.grid(True, alpha=0.3)
    plt.tight_layout()
    
    plt.savefig(output_path, dpi=150, bbox_inches='tight')
    plt.close()
    print(f"Saved: {output_path}")

def generate_frame_time_graph(data, output_path):
    """Generate frame time comparison graph."""
    plt.figure(figsize=(10, 6))
    
    lights = data['lights']
    
    plt.plot(lights, data['forward_ms'], 'b-o', linewidth=2, markersize=8, label='Forward Renderer')
    plt.plot(lights, data['deferred_ms'], 'r-s', linewidth=2, markersize=8, label='Deferred Renderer')
    
    # Mark crossover point
    crossover = find_crossover(data)
    if crossover:
        plt.axvline(x=crossover, color='green', linestyle='--', alpha=0.7, label=f'Crossover (~{crossover} lights)')
    
    plt.xlabel('Number of Lights', fontsize=12)
    plt.ylabel('Frame Time (milliseconds)', fontsize=12)
    plt.title('Forward vs Deferred Rendering: Frame Time by Light Count', fontsize=14)
    plt.legend(loc='best', fontsize=10)
    plt.grid(True, alpha=0.3)
    plt.tight_layout()
    
    plt.savefig(output_path, dpi=150, bbox_inches='tight')
    plt.close()
    print(f"Saved: {output_path}")

def generate_speedup_graph(data, output_path):
    """Generate speedup comparison graph."""
    plt.figure(figsize=(10, 6))
    
    lights = data['lights']
    speedups = data['speedup']
    
    # Color bars based on winner
    colors = ['green' if s > 1.0 else 'blue' for s in speedups]
    
    plt.bar(range(len(lights)), speedups, color=colors, alpha=0.7, edgecolor='black')
    plt.axhline(y=1.0, color='red', linestyle='--', linewidth=2, label='Equal Performance')
    
    plt.xticks(range(len(lights)), lights)
    plt.xlabel('Number of Lights', fontsize=12)
    plt.ylabel('Speedup (Forward/Deferred Time)', fontsize=12)
    plt.title('Deferred Renderer Speedup by Light Count\n(>1.0 = Deferred Faster, <1.0 = Forward Faster)', fontsize=14)
    plt.legend(loc='best', fontsize=10)
    plt.grid(True, alpha=0.3, axis='y')
    plt.tight_layout()
    
    plt.savefig(output_path, dpi=150, bbox_inches='tight')
    plt.close()
    print(f"Saved: {output_path}")

def generate_scaling_graph(data, output_path):
    """Generate performance scaling graph (normalized to 1 light)."""
    plt.figure(figsize=(10, 6))
    
    lights = data['lights']
    
    # Normalize to 1 light baseline
    forward_normalized = [t / data['forward_ms'][0] for t in data['forward_ms']]
    deferred_normalized = [t / data['deferred_ms'][0] for t in data['deferred_ms']]
    
    plt.plot(lights, forward_normalized, 'b-o', linewidth=2, markersize=8, label='Forward Renderer')
    plt.plot(lights, deferred_normalized, 'r-s', linewidth=2, markersize=8, label='Deferred Renderer')
    plt.axhline(y=1.0, color='gray', linestyle='--', alpha=0.5, label='Baseline (1 light)')
    
    plt.xlabel('Number of Lights', fontsize=12)
    plt.ylabel('Relative Frame Time (1 = baseline)', fontsize=12)
    plt.title('Performance Scaling: How Each Renderer Handles More Lights\n(Lower is Better)', fontsize=14)
    plt.legend(loc='best', fontsize=10)
    plt.grid(True, alpha=0.3)
    plt.tight_layout()
    
    plt.savefig(output_path, dpi=150, bbox_inches='tight')
    plt.close()
    print(f"Saved: {output_path}")

def generate_combined_graph(data, output_path):
    """Generate a combined 2x2 subplot graph."""
    fig, axes = plt.subplots(2, 2, figsize=(14, 10))
    
    lights = data['lights']
    crossover = find_crossover(data)
    
    # FPS comparison (top left)
    ax = axes[0, 0]
    ax.plot(lights, data['forward_fps'], 'b-o', linewidth=2, markersize=6, label='Forward')
    ax.plot(lights, data['deferred_fps'], 'r-s', linewidth=2, markersize=6, label='Deferred')
    if crossover:
        ax.axvline(x=crossover, color='green', linestyle='--', alpha=0.7)
    ax.set_xlabel('Number of Lights')
    ax.set_ylabel('FPS')
    ax.set_title('FPS Comparison')
    ax.legend(loc='best')
    ax.grid(True, alpha=0.3)
    
    # Frame time (top right)
    ax = axes[0, 1]
    ax.plot(lights, data['forward_ms'], 'b-o', linewidth=2, markersize=6, label='Forward')
    ax.plot(lights, data['deferred_ms'], 'r-s', linewidth=2, markersize=6, label='Deferred')
    if crossover:
        ax.axvline(x=crossover, color='green', linestyle='--', alpha=0.7)
    ax.set_xlabel('Number of Lights')
    ax.set_ylabel('Frame Time (ms)')
    ax.set_title('Frame Time Comparison')
    ax.legend(loc='best')
    ax.grid(True, alpha=0.3)
    
    # Speedup (bottom left)
    ax = axes[1, 0]
    colors = ['green' if s > 1.0 else 'blue' for s in data['speedup']]
    ax.bar(range(len(lights)), data['speedup'], color=colors, alpha=0.7, edgecolor='black')
    ax.axhline(y=1.0, color='red', linestyle='--', linewidth=2)
    ax.set_xticks(range(len(lights)))
    ax.set_xticklabels(lights)
    ax.set_xlabel('Number of Lights')
    ax.set_ylabel('Speedup')
    ax.set_title('Deferred Speedup (>1 = Deferred Faster)')
    ax.grid(True, alpha=0.3, axis='y')
    
    # Scaling (bottom right)
    ax = axes[1, 1]
    forward_normalized = [t / data['forward_ms'][0] for t in data['forward_ms']]
    deferred_normalized = [t / data['deferred_ms'][0] for t in data['deferred_ms']]
    ax.plot(lights, forward_normalized, 'b-o', linewidth=2, markersize=6, label='Forward')
    ax.plot(lights, deferred_normalized, 'r-s', linewidth=2, markersize=6, label='Deferred')
    ax.axhline(y=1.0, color='gray', linestyle='--', alpha=0.5)
    ax.set_xlabel('Number of Lights')
    ax.set_ylabel('Relative Frame Time')
    ax.set_title('Performance Scaling (1 = baseline)')
    ax.legend(loc='best')
    ax.grid(True, alpha=0.3)
    
    # Add main title
    crossover_text = f"Crossover: ~{crossover} lights" if crossover else "No crossover detected"
    fig.suptitle(f'Forward vs Deferred Rendering: Light Scaling Analysis\n{crossover_text}', 
                 fontsize=16, fontweight='bold')
    
    plt.tight_layout(rect=[0, 0, 1, 0.95])
    plt.savefig(output_path, dpi=150, bbox_inches='tight')
    plt.close()
    print(f"Saved: {output_path}")

def print_text_summary(data):
    """Print a text summary of the results."""
    print("\n" + "="*60)
    print("LIGHT SCALING COMPARISON SUMMARY")
    print("="*60)
    
    print("\n{:>8} {:>12} {:>12} {:>10} {:>10}".format(
        "Lights", "Forward FPS", "Deferred FPS", "Winner", "Speedup"))
    print("-"*60)
    
    for i in range(len(data['lights'])):
        print("{:>8} {:>12.2f} {:>12.2f} {:>10} {:>9.2f}x".format(
            data['lights'][i],
            data['forward_fps'][i],
            data['deferred_fps'][i],
            data['winner'][i],
            data['speedup'][i]
        ))
    
    crossover = find_crossover(data)
    print("-"*60)
    if crossover:
        print(f"\nCROSSOVER POINT: ~{crossover} lights")
        print("(Deferred becomes faster at around this many lights)")
    else:
        print("\nNo crossover detected (Forward always faster)")
    
    # Calculate scaling
    forward_slowdown = data['forward_ms'][-1] / data['forward_ms'][0]
    deferred_slowdown = data['deferred_ms'][-1] / data['deferred_ms'][0]
    
    print(f"\nPERFORMANCE SCALING ({data['lights'][0]} → {data['lights'][-1]} lights):")
    print(f"  Forward slowdown:  {forward_slowdown:.2f}x")
    print(f"  Deferred slowdown: {deferred_slowdown:.2f}x")
    print("="*60)

def main():
    if len(sys.argv) < 2:
        print(__doc__)
        sys.exit(1)
    
    csv_path = sys.argv[1]
    output_dir = sys.argv[2] if len(sys.argv) > 2 else os.path.dirname(csv_path) or '.'
    
    if not os.path.exists(csv_path):
        print(f"Error: CSV file not found: {csv_path}")
        sys.exit(1)
    
    # Load data
    data = load_csv(csv_path)
    print(f"Loaded {len(data['lights'])} data points from {csv_path}")
    
    # Print text summary
    print_text_summary(data)
    
    # Generate graphs if matplotlib is available
    if HAS_MATPLOTLIB:
        print("\nGenerating graphs...")
        os.makedirs(output_dir, exist_ok=True)
        
        generate_fps_comparison_graph(data, os.path.join(output_dir, 'light_scaling_fps.png'))
        generate_frame_time_graph(data, os.path.join(output_dir, 'light_scaling_frame_time.png'))
        generate_speedup_graph(data, os.path.join(output_dir, 'light_scaling_speedup.png'))
        generate_scaling_graph(data, os.path.join(output_dir, 'light_scaling_normalized.png'))
        generate_combined_graph(data, os.path.join(output_dir, 'light_scaling_combined.png'))
        
        print(f"\nAll graphs saved to: {output_dir}/")
    else:
        print("\nNote: Install matplotlib to generate graphs: pip install matplotlib")

if __name__ == "__main__":
    main()

