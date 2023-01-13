import java.time.Duration;
import java.time.Instant;

public class AllocBench {

    public static class Node {
        int value;
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

    public static void main(String... args) {
        Instant instant = Instant.now();

        long iters = 1 << 24;

        for (int i = 0; i < iters; i++) {
            recursiveFunction(100);
        }

        Instant now = Instant.now();

        long ms = Duration.between(instant, now).toMillis();

        System.out.println(ms);

        System.out.println("Allocs per sec (in millions) " + ((100L * iters) / ms * 1000) / 1000000.0);
    }
}
