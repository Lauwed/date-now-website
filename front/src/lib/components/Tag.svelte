<script lang="ts">
	import type { Snippet } from 'svelte';

	interface Props {
		tag?: string;
		children: Snippet;
	}

	let { tag = 'p', children }: Props = $props();
</script>

<svelte:element this={tag} class="tag">
	{@render children()}
</svelte:element>

<style lang="scss">
	@use './../styles/mixins' as *;
	@use './../styles/variables' as *;

	.tag {
		@include glass($box-shadow: none, $color: var(--color, $whiteRGB));
		font-size: 0.875rem;
		padding: 2px 12px;
		display: inline-block;
		border-radius: 20px;
		margin: var(--margin, 0);
	}
</style>
