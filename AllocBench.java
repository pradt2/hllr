import java.time.Duration;
import java.time.Instant;

public class AllocBench {

    public static class Node {
        static int total;
        int value;

        public Node() {
            Node.total++;
        }
    }

    private static Node recursiveFunction(int recursionLevel) {

        Node node = new Node();
        node.value = recursionLevel;

        Node node1;

        if (recursionLevel > 0) {
            node1 = recursiveFunction(recursionLevel - 1);
        } else return node;

        return node.hashCode() < node1.hashCode() ? node : node1;
    }

    public static void main2(String... args) {
        Instant instant = Instant.now();

        long iters = 1 << 24;
        int recursion = 100;

        for (int i = 0; i < iters; i++) {
            recursiveFunction(recursion);
        }

        Instant now = Instant.now();

        long ms = Duration.between(instant, now).toMillis();

        System.out.println(ms);

        System.out.println("Allocs per sec (in millions) " + ((recursion * iters) / ms * 1000) / 1000000.0);
    }

    private static int size = 1024;
    public static Node[] nodes1 = new Node[size];
    public static Node[] nodes2 = new Node[size];
    public static Node[] nodes3 = new Node[size];
    public static Node[] nodes4 = new Node[size];


    public static void printLowest(Node[] nodes) {
        try {
            Thread.sleep(4000);
        } catch (Exception e) {
            throw new RuntimeException(e);
        }

        Node lowest = null;
        for (Node node : nodes) {
            if (lowest == null) {
                lowest = node;
                continue;
            }
            if (lowest.hashCode() > node.hashCode()) lowest = node;
        }
        System.out.println(lowest);
    }

    static void loop(Node[] nodes) {
        long iters = 1024 * 1024 * 3;

        Node lowest = new Node();

        for (long iter = 0; iter < iters; iter++) {
            for (int i = 0; i < size; i++) {
                nodes[i] = new Node();
            }
        }
    }

    public static void main(String... args) throws Exception {

//         new Thread(AllocBench::printLowest).start();

        Instant instant = Instant.now();

        long iters = 1024 * 1024 * 3;

        Node lowest = new Node();

        Thread t1 = new Thread(() -> AllocBench.loop(nodes1));
        t1.start();
        Thread t2 = new Thread(() -> AllocBench.loop(nodes2));
        t2.start();
        Thread t3 = new Thread(() -> AllocBench.loop(nodes3));
        t3.start();
        Thread t4 = new Thread(() -> AllocBench.loop(nodes4));
        t4.start();

        int threadCount = 4;

        t1.join();
        t2.join();
        t3.join();
        t4.join();

        Instant now = Instant.now();

        long ms = Duration.between(instant, now).toMillis();

        System.out.println(ms);

        System.out.println("Allocs per sec (in millions) " + ((size * threadCount * iters) / ms * 1000) / 1000000.0);
    }
}
