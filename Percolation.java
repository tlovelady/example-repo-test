/* *****************************************************************************
 *  Name:              Alan Turing
 *  Coursera User ID:  123456
 *  Last modified:     1/1/2019
 **************************************************************************** */

import edu.princeton.cs.algs4.WeightedQuickUnionUF;

public class Percolation {

    private int[] grid;
    private int size;
    private WeightedQuickUnionUF quickUnion;

    // Percolation related methods
    // creates n-by-n grid, with all sites initially blocked
    public Percolation(int n) {

        grid = new int[n * n];
        size = n;

        for (int i : grid) grid[i] = 0;

        quickUnion = new WeightedQuickUnionUF((size * size) + 1);
        for (int i = 0; i < 5; i++) {
            quickUnion.union(i, quickUnion.count() - 1);
        }
        //make count a variable
        for (int i = quickUnion.count() - 3; i > quickUnion.count() - 8; i--) {
            quickUnion.union(i, quickUnion.count() - 2);
        }
    }

    // opens the site (row, col) if it is not open already
    public void open(int row, int col) {

        int index = getIndexFromRowAndCol(row, col);
        grid[index] = 1;
        if (row > 1) {
            if (isOpen(row - 1, col)) {
                quickUnion.union(getIndexFromRowAndCol(row, col),
                                 getIndexFromRowAndCol(row - 1, col));
            }
        }
        if (row < size) {
            if (isOpen(row + 1, col)) {
                quickUnion.union(getIndexFromRowAndCol(row, col),
                                 getIndexFromRowAndCol(row + 1, col));
            }
        }
        if (col > 1) {
            if (isOpen(row, col - 1)) {
                quickUnion.union(getIndexFromRowAndCol(row, col),
                                 getIndexFromRowAndCol(row, col - 1));
            }
        }
        if (col < size) {
            if (isOpen(row, col + 1)) {
                quickUnion.union(getIndexFromRowAndCol(row, col),
                                 getIndexFromRowAndCol(row, col + 1));
            }
        }
    }

    // is the site (row, col) open?
    public boolean isOpen(int row, int col) {

        int index = getIndexFromRowAndCol(row, col);
        if (grid[index] == 1) return true;
        return false;
    }

    // is the site (row, col) full?
    public boolean isFull(int row, int col) {

        return !isOpen(row, col);
    }

    // returns the number of open sites
    public int numberOfOpenSites() {

        int num = 0;
        for (int i : grid) {
            if (i == 1) num++;
        }
        return num;
    }

    // does the system percolate?
    public boolean percolates() {
        if (quickUnion.find(quickUnion.count() - 1) == quickUnion.find(quickUnion.count() - 2)) return true;
        return false;
    }

    // Union related methods
    private int getIndexFromRowAndCol(int row, int col) {
        return (col + ((row - 1) * size)) - 1;
    }

    // test client (optional)
    public static void main(String[] args) {
        Percolation perc = new Percolation(5);
        for (int i = 1; i < 6; i++) {
            for (int j = 1; j < 6; j++) {
                System.out.println(perc.isOpen(i, j));
            }
            System.out.print("/n");
        }
    }

}
