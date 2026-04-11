<script lang="ts">
	import Block from './Block.svelte';
	import type { Block as BlockType } from '../../../types';

	let { tag, content, attributes, children, ...rest }: BlockType = $props();
</script>

<svelte:element this={tag} class="block" {...attributes}>
	{#if content}
		{content}
	{:else if children}
		{#each children as child}
			<Block {...child} />
		{/each}
	{/if}
</svelte:element>

<style lang="scss">
</style>
