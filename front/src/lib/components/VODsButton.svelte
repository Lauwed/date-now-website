<script lang="ts">
	import { PUBLIC_YOUTUBE_URL } from '$env/static/public';
	import Button from './Button.svelte';

	interface Props {
		icon?: boolean;
	}

	let { icon = false }: Props = $props();
</script>

{#if icon}
	<Button isIcon tag="a" href={PUBLIC_YOUTUBE_URL} title="Youtube">
		<svg xmlns="http://www.w3.org/2000/svg" viewBox="0 0 28.57 20"
			><path
				fill="currentColor"
				d="M27.973 3.123A3.58 3.58 0 0 0 25.447.597C23.22 0 14.285 0 14.285 0S5.35 0 3.123.597A3.58 3.58 0 0 0 .597 3.123C0 5.35 0 10 0 10s0 4.65.597 6.877a3.58 3.58 0 0 0 2.526 2.526C5.35 20 14.285 20 14.285 20s8.935 0 11.162-.597a3.58 3.58 0 0 0 2.526-2.526C28.57 14.65 28.57 10 28.57 10s-.002-4.65-.597-6.877"
			/><path fill="#fff" d="M11.425 14.285 18.848 10l-7.423-4.285z" /></svg
		>
	</Button>
{:else}
	<Button tag="a" href={PUBLIC_YOUTUBE_URL}>VODs on YouTube</Button>
{/if}

<style lang="scss">
	svg {
		width: 18px;
		aspect-ratio: 1/1;
	}
</style>
