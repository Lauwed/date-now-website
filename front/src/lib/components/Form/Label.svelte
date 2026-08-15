<script lang="ts">
	import type { Snippet } from 'svelte';

	interface Props {
		children: Snippet;
		forHTML: string;
	}

	let { children, forHTML }: Props = $props();
</script>

<label for={forHTML}>{@render children()}</label>

<style lang="scss"></style>
