<script lang="ts">
	import type { Block as BlockType } from '../../../types';
	import Block from './Block.svelte';

	interface Props {
		blocks: BlockType[];
	}

	let { blocks }: Props = $props();
</script>

<div class="block-renderer">
	{#each blocks as block}
		<Block {...block} />
	{/each}
</div>

<style lang="scss">
</style>
