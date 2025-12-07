#!/usr/bin/env python3
"""
Generate comparison graphs for NFL renderer comparison results
"""

import pandas as pd
import matplotlib.pyplot as plt
import sys
import os

def generate_graphs(csv_path, output_dir="."):
    """Generate comparison graphs from CSV data"""
    
    # Read CSV data
    df = pd.read_csv(csv_path)
    
    # Create output directory if needed
    os.makedirs(output_dir, exist_ok=True)
    
    # Set up the plot style
    plt.style.use('seaborn-v0_8-darkgrid')
    fig, axes = plt.subplots(2, 2, figsize=(14, 10))
    fig.suptitle('NFL Renderer Comparison: Forward vs Deferred', fontsize=16, fontweight='bold')
    
    # Extract data
    forward = df[df['Renderer'] == 'Forward'].iloc[0]
    deferred = df[df['Renderer'] == 'Deferred'].iloc[0] if len(df[df['Renderer'] == 'Deferred']) > 0 else None
    
    if deferred is None:
        print("Warning: Deferred renderer data not available")
        return
    
    renderers = ['Forward', 'Deferred']
    colors = ['#3498db', '#e74c3c']
    
    # 1. Average Frame Time Comparison
    ax1 = axes[0, 0]
    times = [forward['AvgFrameTimeMs'], deferred['AvgFrameTimeMs']]
    bars = ax1.bar(renderers, times, color=colors, alpha=0.7, edgecolor='black', linewidth=1.5)
    ax1.set_ylabel('Average Frame Time (ms)', fontsize=12)
    ax1.set_title('Average Frame Time', fontsize=14, fontweight='bold')
    ax1.grid(True, alpha=0.3)
    
    # Add value labels on bars
    for bar, time in zip(bars, times):
        height = bar.get_height()
        ax1.text(bar.get_x() + bar.get_width()/2., height,
                f'{time:.2f} ms',
                ha='center', va='bottom', fontsize=11, fontweight='bold')
    
    # 2. FPS Comparison
    ax2 = axes[0, 1]
    fps_values = [forward['FPS'], deferred['FPS']]
    bars = ax2.bar(renderers, fps_values, color=colors, alpha=0.7, edgecolor='black', linewidth=1.5)
    ax2.set_ylabel('Frames Per Second (FPS)', fontsize=12)
    ax2.set_title('Rendering FPS', fontsize=14, fontweight='bold')
    ax2.grid(True, alpha=0.3)
    
    # Add value labels on bars
    for bar, fps in zip(bars, fps_values):
        height = bar.get_height()
        ax2.text(bar.get_x() + bar.get_width()/2., height,
                f'{fps:.2f}',
                ha='center', va='bottom', fontsize=11, fontweight='bold')
    
    # 3. Min/Max Frame Time Range
    ax3 = axes[1, 0]
    forward_range = [forward['MinFrameTimeMs'], forward['MaxFrameTimeMs']]
    deferred_range = [deferred['MinFrameTimeMs'], deferred['MaxFrameTimeMs']]
    
    x_pos = [0, 1]
    ax3.errorbar(x_pos[0], forward['AvgFrameTimeMs'], 
                yerr=[[forward['AvgFrameTimeMs'] - forward['MinFrameTimeMs']], 
                      [forward['MaxFrameTimeMs'] - forward['AvgFrameTimeMs']]],
                fmt='o', color=colors[0], markersize=10, capsize=10, capthick=2,
                label='Forward', linewidth=2)
    ax3.errorbar(x_pos[1], deferred['AvgFrameTimeMs'],
                yerr=[[deferred['AvgFrameTimeMs'] - deferred['MinFrameTimeMs']],
                      [deferred['MaxFrameTimeMs'] - deferred['AvgFrameTimeMs']]],
                fmt='o', color=colors[1], markersize=10, capsize=10, capthick=2,
                label='Deferred', linewidth=2)
    ax3.set_xticks(x_pos)
    ax3.set_xticklabels(renderers)
    ax3.set_ylabel('Frame Time (ms)', fontsize=12)
    ax3.set_title('Frame Time Range (Min-Max)', fontsize=14, fontweight='bold')
    ax3.legend()
    ax3.grid(True, alpha=0.3)
    
    # 4. Performance Comparison (Speedup)
    ax4 = axes[1, 1]
    speedup = forward['TotalTimeMs'] / deferred['TotalTimeMs']
    fps_improvement = ((deferred['FPS'] / forward['FPS']) - 1.0) * 100.0
    
    metrics = ['Speedup\n(Total Time)', 'FPS\nImprovement %']
    values = [speedup, fps_improvement]
    bars = ax4.bar(metrics, values, color=['#2ecc71', '#f39c12'], alpha=0.7, edgecolor='black', linewidth=1.5)
    ax4.set_ylabel('Value', fontsize=12)
    ax4.set_title('Performance Metrics', fontsize=14, fontweight='bold')
    ax4.axhline(y=1.0, color='red', linestyle='--', linewidth=1, label='Baseline (1x)')
    ax4.axhline(y=0, color='black', linestyle='-', linewidth=0.5)
    ax4.legend()
    ax4.grid(True, alpha=0.3)
    
    # Add value labels
    for bar, val in zip(bars, values):
        height = bar.get_height()
        ax4.text(bar.get_x() + bar.get_width()/2., height,
                f'{val:.2f}' + ('x' if val == speedup else '%'),
                ha='center', va='bottom' if height > 0 else 'top', fontsize=11, fontweight='bold')
    
    plt.tight_layout()
    
    # Save figure
    output_path = os.path.join(output_dir, 'nfl_renderer_comparison.png')
    plt.savefig(output_path, dpi=300, bbox_inches='tight')
    print(f"Graphs saved to: {output_path}")
    
    # Also create a summary text file
    summary_path = os.path.join(output_dir, 'nfl_renderer_comparison_summary.txt')
    with open(summary_path, 'w') as f:
        f.write("NFL Renderer Comparison Summary\n")
        f.write("=" * 50 + "\n\n")
        f.write(f"Forward Renderer:\n")
        f.write(f"  Total Time: {forward['TotalTimeMs']:.2f} ms ({forward['TotalTimeMs']/1000:.2f} s)\n")
        f.write(f"  Avg Frame Time: {forward['AvgFrameTimeMs']:.2f} ms\n")
        f.write(f"  FPS: {forward['FPS']:.2f}\n")
        f.write(f"  Min Frame Time: {forward['MinFrameTimeMs']:.2f} ms\n")
        f.write(f"  Max Frame Time: {forward['MaxFrameTimeMs']:.2f} ms\n\n")
        
        f.write(f"Deferred Renderer:\n")
        f.write(f"  Total Time: {deferred['TotalTimeMs']:.2f} ms ({deferred['TotalTimeMs']/1000:.2f} s)\n")
        f.write(f"  Avg Frame Time: {deferred['AvgFrameTimeMs']:.2f} ms\n")
        f.write(f"  FPS: {deferred['FPS']:.2f}\n")
        f.write(f"  Min Frame Time: {deferred['MinFrameTimeMs']:.2f} ms\n")
        f.write(f"  Max Frame Time: {deferred['MaxFrameTimeMs']:.2f} ms\n\n")
        
        f.write(f"Comparison:\n")
        f.write(f"  Speedup: {speedup:.2f}x\n")
        f.write(f"  FPS Improvement: {fps_improvement:.1f}%\n")
        f.write(f"  Winner: {'Deferred' if speedup > 1.0 else 'Forward'}\n")
    
    print(f"Summary saved to: {summary_path}")

if __name__ == "__main__":
    if len(sys.argv) < 2:
        print("Usage: python3 generate_nfl_comparison_graphs.py <csv_file> [output_dir]")
        sys.exit(1)
    
    csv_path = sys.argv[1]
    output_dir = sys.argv[2] if len(sys.argv) > 2 else "."
    
    if not os.path.exists(csv_path):
        print(f"Error: CSV file not found: {csv_path}")
        sys.exit(1)
    
    generate_graphs(csv_path, output_dir)

