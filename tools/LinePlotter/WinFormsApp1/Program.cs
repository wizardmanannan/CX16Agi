using System;
using System.Collections.Generic;
using System.Drawing;
using System.IO;
using System.Linq;
using System.Windows.Forms;

namespace LineDrawer
{
    public partial class MainForm : Form
    {
        private List<Tuple<Point, Point?, int?>> linesAndPoints;
        private const int ScaleFactor = 15; // Scale factor to make drawing larger

        public MainForm()
        {
            InitializeComponent();
            linesAndPoints = new List<Tuple<Point, Point?, int?>>();
            this.Paint += new PaintEventHandler(MainForm_Paint);
            this.Load += new EventHandler(MainForm_Load);
            this.AutoScroll = true; // Enable scrolling
        }

        private void MainForm_Load(object sender, EventArgs e)
        {
            string[] fileLines = File.ReadAllLines("lines.txt");
            int lineNumber = 1;

            foreach (var line in fileLines)
            {
                var points = ParseLineOrPoint(line, ref lineNumber);
                if (points != null)
                {
                    linesAndPoints.Add(points);
                }
            }

            this.AutoScrollMinSize = new Size(ScaleFactor * 200, ScaleFactor * 200); // Set the minimum size for scrolling
            this.Invalidate(); // Force the form to repaint
        }

        private Tuple<Point, Point?, int?> ParseLineOrPoint(string line, ref int lineNumber)
        {
            try
            {
                var parts = line.Split(new[] { ' ', '.' }, StringSplitOptions.RemoveEmptyEntries);
                if (parts.Length == 4)
                {
                    // Single point format: X1 0 Y1 0
                    int x = int.Parse(parts[1]);
                    int y = int.Parse(parts[3]);
                    return new Tuple<Point, Point?, int?>(new Point(x * ScaleFactor, y * ScaleFactor), null, lineNumber++);
                }
                else if (parts.Length == 9)
                {
                    // Line format: arbitraryNumber. X 0 Y 0 X2 159 Y2 0
                    int x1 = int.Parse(parts[2]);
                    int y1 = int.Parse(parts[4]);
                    int x2 = int.Parse(parts[6]);
                    int y2 = int.Parse(parts[8]);
                    return new Tuple<Point, Point?, int?>(new Point(x1 * ScaleFactor, y1 * ScaleFactor), new Point(x2 * ScaleFactor, y2 * ScaleFactor), lineNumber++);
                }
            }
            catch
            {
                return null;
            }
            return null;
        }

        private void MainForm_Paint(object sender, PaintEventArgs e)
        {
            Graphics g = e.Graphics;
            g.TranslateTransform(this.AutoScrollPosition.X, this.AutoScrollPosition.Y); // Adjust for scrolling
            foreach (var item in linesAndPoints)
            {
                Point p1 = item.Item1;
                Point? p2 = item.Item2;
                int? lineNumber = item.Item3;

                if (p2.HasValue)
                {
                    g.DrawLine(Pens.Black, p1, p2.Value);
                    DrawLineNumber(g, p1, p2.Value, lineNumber.Value);
                }
                else
                {
                    g.FillEllipse(Brushes.Black, p1.X - 2, p1.Y - 2, 4, 4); // Draw point as small circle
                    DrawPointNumber(g, p1, lineNumber.Value);
                }
            }
        }

        private void DrawLineNumber(Graphics g, Point p1, Point p2, int lineNumber)
        {
            float midX = (p1.X + p2.X) / 2;
            float midY = (p1.Y + p2.Y) / 2;
            g.DrawString(lineNumber.ToString(), this.Font, Brushes.Red, midX, midY);
        }

        private void DrawPointNumber(Graphics g, Point p, int lineNumber)
        {
            g.DrawString(lineNumber.ToString(), this.Font, Brushes.Red, p.X + 5, p.Y); // Draw the number near the point
        }

        [STAThread]
        static void Main()
        {
            Application.EnableVisualStyles();
            Application.SetCompatibleTextRenderingDefault(false);
            Application.Run(new MainForm());
        }
    }

    partial class MainForm : Form
    {
        private void InitializeComponent()
        {
            this.SuspendLayout();
            // 
            // MainForm
            // 
            this.ClientSize = new System.Drawing.Size(800, 450);
            this.Name = "MainForm";
            this.Text = "Line Drawer";
            this.WindowState = FormWindowState.Maximized; // Maximize the window
            this.ResumeLayout(false);
        }
    }
}
